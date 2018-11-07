/* -*- C++ -*-; c-basic-offset: 4; indent-tabs-mode: nil */
/*
 * Copyright (c) 2017 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#ifndef OPFLEXAGENT_VPPMANAGER_H_
#define OPFLEXAGENT_VPPMANAGER_H_

#include <boost/asio/deadline_timer.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/noncopyable.hpp>
#include <boost/optional.hpp>

#include <vom/acl_ethertype.hpp>
#include <vom/acl_list.hpp>
#include <vom/hw.hpp>
#include <vom/interface.hpp>
#include <vom/interface_cmds.hpp>

#include <opflex/ofcore/PeerStatusListener.h>

#include <utility>

#include "opflexagent/Agent.h"
#include "opflexagent/EndpointManager.h"
#include "opflexagent/IdGenerator.h"
#include "opflexagent/RDConfig.h"
#include "opflexagent/TaskQueue.h"

#include "VppCrossConnect.h"
#include "VppUplink.h"
#include "VppVirtualRouter.h"

/*
 * Forward declare classes to reduce compile time coupling
 */
namespace VOM {
class bridge_domain;
class gbp_endpoint_group;
};

namespace opflexagent {

/**
 * @brief Makes changes to VPP to be in sync with state of MOs.
 * Main function is to handling change notifications, generate a set
 * of config modifications that represent the changes and apply these
 * modifications.
 */
class VppManager : public EndpointListener,
                   public ServiceListener,
                   public ExtraConfigListener,
                   public PolicyListener,
                   public opflex::ofcore::PeerStatusListener,
                   public VOM::interface::event_listener,
                   public VOM::interface::stat_listener,
                   private boost::noncopyable {
public:
    /**
     * Construct a new Vpp manager for the agent
     * @param agent the agent object
     * @param idGen the flow ID generator
     */
    VppManager(Agent& agent, IdGenerator& idGen, VOM::HW::cmd_q* q);

    ~VppManager() {}

    /**
     * Module start
     */
    virtual void start();

    /**
     * Installs listeners for receiving updates to MODB state.
     */
    virtual void registerModbListeners();

    /**
     * Module stop
     */
    virtual void stop();

    /**
     * Enable or disable the virtual routing
     *
     * @param virtualRouterEnabled true to enable the router
     * @param routerAdv true to enable IPv6 router advertisements
     * @param mac the MAC address to use as the router MAC formatted
     * as a colon-separated string of 6 hex-encoded bytes.
     */
    void setVirtualRouter(bool virtualRouterEnabled, bool routerAdv,
                          const std::string& mac);

    /* Interface: EndpointListener */
    virtual void endpointUpdated(const std::string& uuid);

    /* Interface: ServiceListener */
    virtual void serviceUpdated(const std::string& uuid);

    /* Interface: ExtraConfigListener */
    virtual void rdConfigUpdated(const opflex::modb::URI& rdURI);

    /* Interface: PolicyListener */
    virtual void egDomainUpdated(const opflex::modb::URI& egURI);
    virtual void domainUpdated(opflex::modb::class_id_t cid,
                               const opflex::modb::URI& domURI);
    virtual void contractUpdated(const opflex::modb::URI& contractURI);
    virtual void configUpdated(const opflex::modb::URI& configURI);

    virtual void secGroupSetUpdated(const EndpointListener::uri_set_t& secGrps);
    virtual void secGroupUpdated(const opflex::modb::URI&);

    /* Interface: PortStatusListener */
    virtual void portStatusUpdate(const std::string& portName, uint32_t portNo,
                                  bool fromDesc);

    /**
     * Implementation for PeerStatusListener::peerStatusUpdated
     *
     * @param peerHostname the host name for the connection
     * @param peerPort the port number for the connection
     * @param peerStatus the new status for the connection
     */
    virtual void peerStatusUpdated(const std::string& peerHostname,
                                   int peerPort, PeerStatus peerStatus);

    /**
     * Get or generate a unique ID for a given object for use with flows.
     *
     * @param cid Class ID of the object
     * @param uri URI of the object
     * @return A unique ID for the object
     */
    uint32_t getId(opflex::modb::class_id_t cid, const opflex::modb::URI& uri);

    /**
     * Return the uplink object
     */
    VPP::Uplink& uplink();

    /**
     * Return the cross connect object
     */
    VPP::CrossConnect& crossConnect();
private:
    /**
     * Compare and update changes in an endpoint.
     *
     * @param uuid UUID of the changed endpoint
     */
    void handleEndpointUpdate(const std::string& uuid);

    /**
     * Compare and update changes in an endpoint group.
     *
     * @param egURI URI of the changed endpoint group
     */
    void handleEndpointGroupDomainUpdate(const opflex::modb::URI& egURI);

    /**
     * Update given routing domain
     *
     * @param rdURI URI of the changed routing domain
     */
    void handleRoutingDomainUpdate(const opflex::modb::URI& rdURI);

    /**
     * Handle changes to a forwarding domain; only deals with
     * cleaning up when these objects are removed.
     *
     * @param cid Class of the forwarding domain
     * @param domURI URI of the changed forwarding domain
     */
    void handleDomainUpdate(opflex::modb::class_id_t cid,
                            const opflex::modb::URI& domURI);

    /**
     * Compare and update changes in a contract
     *
     * @param contractURI URI of the changed contract
     */
    void handleContractUpdate(const opflex::modb::URI& contractURI);

    /**
     * Openstack Security Group
     */
    void handleSecGrpUpdate(const opflex::modb::URI& uri);

    /**
     * Openstack Security Group Set
     */
    void handleSecGrpSetUpdate(const EndpointListener::uri_set_t& secGrps);
    void allowDhcpRequest(VOM::ACL::l3_list::rules_t& in_rules,
                          VOM::ACL::l3_list::rules_t& out_rules,
                          uint16_t etherType);
    void buildSecGrpSetUpdate(const uri_set_t& secGrps,
                              const std::string& secGrpId,
                              VOM::ACL::l3_list::rules_t& in_rules,
                              VOM::ACL::l3_list::rules_t& out_rules,
                              VOM::ACL::acl_ethertype::ethertype_rules_t&
                              ethertype_rules);
    /**
     * Compare and update changes in platform config
     *
     * @param configURI URI of the changed contract
     */
    void handleConfigUpdate(const opflex::modb::URI& configURI);

    /**
     * Handle changes to port-status for endpoints and endpoint groups.
     *
     * @param portName Name of the port that changed
     * @param portNo Port number of the port that changed
     */
    void handlePortStatusUpdate(const std::string& portName, uint32_t portNo);

    /**
     * Get the VNID for the specified endpoint groups or L3 external
     * networks
     *
     * @param uris URIs of endpoint groups to search for
     * @param ids the corresponding set of vnids
     */
    void getGroupVnid(const std::unordered_set<opflex::modb::URI>& uris,
                      std::unordered_set<uint32_t>& ids);
    uint32_t getExtNetVnid(const opflex::modb::URI& uri);

    bool getGroupForwardingInfo(const opflex::modb::URI& egUri, uint32_t& vnid,
                                boost::optional<opflex::modb::URI>& rdURI,
                                uint32_t& rdId,
                                boost::optional<opflex::modb::URI>& bdURI,
                                uint32_t& bdId);
    void updateGroupSubnets(const opflex::modb::URI& egUri, uint32_t bdId,
                            uint32_t rdId);
    void updateEPGFlood(const opflex::modb::URI& epgURI, uint32_t epgVnid,
                        uint32_t fgrpId, boost::asio::ip::address epgTunDst);

    /**
     * Event listener override to get Interface events
     */
    void handle_interface_event(VOM::interface_cmds::events_cmd* e);

    /**
     * Handle interface event in the task-queue context
     */
    void handleInterfaceEvent(VOM::interface_cmds::events_cmd* e);

    /**
     * Event listener override to get Interface stats
     */
    void handle_interface_stat(VOM::interface_cmds::stats_enable_cmd* e);

    /**
     * Handle interface stats in the task-queue context
     */
    void handleInterfaceStat(VOM::interface_cmds::stats_enable_cmd* e);

    /**
     * Handle the connect request to VPP
     */
    void handleInitConnection();

    /**
     * Handle a disconnect from VPP request
     */
    void handleCloseConnection();

    /**
     * Handle the connect request to VPP
     */
    void handleUplinkConfigure();

    /**
     * Handle the cross connect requests to VPP
     */
    void handleXConnectConfigure();

    /**
     * Handle the Vpp Boot request
     */
    void handleBoot();

    /**
     * Import the subnets from an RD into the VRF of a EPG
     */
    std::shared_ptr<gbp_endpoint_group> getEndPointGroup(const std::string& uuid,
                                                         const opflex::modb::URI &egpURI);

    /**
     * Get the subnet internal to a route domain
     */
    network::subnets_t getRDSubnets(const opflex::modb::URI& rdURI);

    /**
     * Handle the Vpp sweep timeout
     */
    void handleSweepTimer(const boost::system::error_code& ec);

    /**
     * Handle the HW poll timeout
     */
    void handleHWPollTimer(const boost::system::error_code& ec);

    /**
     * Referene to the uber-agent
     */
    Agent& agent;

    /**
     * Refernece to the ID generator instance
     */
    IdGenerator& idGen;

    /**
     * The internal task-queue for handling the async upates
     */
    TaskQueue taskQueue;

    /**
     * Virtual Router Settings
     */
    std::shared_ptr<VPP::VirtualRouter> m_vr;

    /**
     * The sweep boot state timer.
     *  This is a member here so it has access to the taskQ
     */
    std::unique_ptr<boost::asio::deadline_timer> m_sweep_timer;

    /**
     * Uplink interface manager
     */
    VPP::Uplink m_uplink;

    /**
     * CrossConnect interface manager
     */
    VPP::CrossConnect m_xconnect;

    /**
     * A list of interest/want commands
     */
    std::list<std::shared_ptr<VOM::cmd>> m_cmds;

    /**
     * The HW poll timer
     */
    std::unique_ptr<boost::asio::deadline_timer> m_poll_timer;

    /**
     * Return the namespace name from the ID
     */
    const char* getIdNamespace(opflex::modb::class_id_t cid);

    /**
     * indicator this manager is stopping
     */
    volatile bool stopping;

    /**
     * indicator for hw liveness
     */
    bool hw_connected;

    void initPlatformConfig();
};

} // namespace opflexagent

/*
 * Local Variables:
 * eval: (c-set-style "llvm.org")
 * End:
 */

#endif // VPPAGENT_VPPMANAGER_H_