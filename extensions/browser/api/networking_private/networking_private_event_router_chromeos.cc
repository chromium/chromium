// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_event_router.h"

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_certificate_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/onc/onc_translator.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/networking_private.h"

using ::ash::DeviceState;
using ::ash::NetworkHandler;
using ::ash::NetworkState;
using ::ash::NetworkStateHandler;

namespace extensions {

namespace {

api::networking_private::CaptivePortalStatus GetCaptivePortalStatus(
    const NetworkState* network) {
  if (!network) {
    return api::networking_private::CaptivePortalStatus::kUnknown;
  }
  if (!network->IsConnectedState()) {
    return api::networking_private::CaptivePortalStatus::kOffline;
  }
  switch (network->GetPortalState()) {
    case NetworkState::PortalState::kUnknown:
      return api::networking_private::CaptivePortalStatus::kUnknown;
    case NetworkState::PortalState::kOnline:
      return api::networking_private::CaptivePortalStatus::kOnline;
    case NetworkState::PortalState::kPortalSuspected:
    case NetworkState::PortalState::kPortal:
    case NetworkState::PortalState::kNoInternet:
      return api::networking_private::CaptivePortalStatus::kPortal;
  }
}

}  // namespace

class NetworkingPrivateEventRouterImpl
    : public NetworkingPrivateEventRouter,
      public ash::NetworkStateHandlerObserver,
      public ash::NetworkCertificateHandler::Observer {
 public:
  explicit NetworkingPrivateEventRouterImpl(content::BrowserContext* context);

  NetworkingPrivateEventRouterImpl(const NetworkingPrivateEventRouterImpl&) =
      delete;
  NetworkingPrivateEventRouterImpl& operator=(
      const NetworkingPrivateEventRouterImpl&) = delete;

  ~NetworkingPrivateEventRouterImpl() override;

 protected:
  // KeyedService overrides:
  void Shutdown() override;

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // NetworkStateHandlerObserver overrides:
  void NetworkListChanged() override;
  void DeviceListChanged() override;
  void PortalStateChanged(const NetworkState* default_network,
                          NetworkState::PortalState portal_state) override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void DevicePropertiesUpdated(const DeviceState* device) override;
  void ScanCompleted(const DeviceState* device) override;

  // NetworkCertificateHandler::Observer overrides:
  void OnCertificatesChanged() override;

 private:
  // Decide if we should listen for network changes or not. If there are any
  // JavaScript listeners registered for the onNetworkChanged event, then we
  // want to register for change notification from the network state handler.
  // Otherwise, we want to unregister and not be listening to network changes.
  void StartOrStopListeningForNetworkChanges();

  raw_ptr<content::BrowserContext> context_;
  bool listening_ = false;
};

NetworkingPrivateEventRouterImpl::NetworkingPrivateEventRouterImpl(
    content::BrowserContext* context)
    : context_(context) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router) {
    event_router->RegisterObserver(
        this, api::networking_private::OnNetworksChanged::kEventName);
    event_router->RegisterObserver(
        this, api::networking_private::OnNetworkListChanged::kEventName);
    event_router->RegisterObserver(
        this, api::networking_private::OnDeviceStateListChanged::kEventName);
    event_router->RegisterObserver(
        this, api::networking_private::OnPortalDetectionCompleted::kEventName);
    event_router->RegisterObserver(
        this, api::networking_private::OnCertificateListsChanged::kEventName);
    StartOrStopListeningForNetworkChanges();
  }
}

NetworkingPrivateEventRouterImpl::~NetworkingPrivateEventRouterImpl() {
  DCHECK(!listening_);
}

void NetworkingPrivateEventRouterImpl::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all context services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router) {
    event_router->UnregisterObserver(this);
  }

  if (listening_) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
  listening_ = false;
}

void NetworkingPrivateEventRouterImpl::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to events from the network state handler.
  StartOrStopListeningForNetworkChanges();
}

void NetworkingPrivateEventRouterImpl::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events from the network state handler if there are no
  // more listeners.
  StartOrStopListeningForNetworkChanges();
}

void NetworkingPrivateEventRouterImpl::StartOrStopListeningForNetworkChanges() {
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen =
      event_router->HasEventListener(
          api::networking_private::OnNetworksChanged::kEventName) ||
      event_router->HasEventListener(
          api::networking_private::OnNetworkListChanged::kEventName) ||
      event_router->HasEventListener(
          api::networking_private::OnDeviceStateListChanged::kEventName) ||
      event_router->HasEventListener(
          api::networking_private::OnPortalDetectionCompleted::kEventName) ||
      event_router->HasEventListener(
          api::networking_private::OnCertificateListsChanged::kEventName);

  if (should_listen && !listening_) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
    NetworkHandler::Get()->network_certificate_handler()->AddObserver(this);
  } else if (!should_listen && listening_) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
    NetworkHandler::Get()->network_certificate_handler()->RemoveObserver(this);
  }
  listening_ = should_listen;
}

void NetworkingPrivateEventRouterImpl::NetworkListChanged() {
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          api::networking_private::OnNetworkListChanged::kEventName)) {
    return;
  }

  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkList(
      &networks);
  std::vector<std::string> changes;
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin();
       iter != networks.end(); ++iter) {
    changes.push_back((*iter)->guid());
  }

  auto args(api::networking_private::OnNetworkListChanged::Create(changes));
  std::unique_ptr<Event> extension_event(
      new Event(events::NETWORKING_PRIVATE_ON_NETWORK_LIST_CHANGED,
                api::networking_private::OnNetworkListChanged::kEventName,
                std::move(args)));
  event_router->BroadcastEvent(std::move(extension_event));
}

void NetworkingPrivateEventRouterImpl::DeviceListChanged() {
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          api::networking_private::OnDeviceStateListChanged::kEventName)) {
    return;
  }

  auto args(api::networking_private::OnDeviceStateListChanged::Create());
  std::unique_ptr<Event> extension_event(
      new Event(events::NETWORKING_PRIVATE_ON_DEVICE_STATE_LIST_CHANGED,
                api::networking_private::OnDeviceStateListChanged::kEventName,
                std::move(args)));
  event_router->BroadcastEvent(std::move(extension_event));
}

void NetworkingPrivateEventRouterImpl::NetworkPropertiesUpdated(
    const NetworkState* network) {
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          api::networking_private::OnNetworksChanged::kEventName)) {
    NET_LOG(EVENT)
        << "NetworkingPrivate.NetworkPropertiesUpdated: No Listeners: "
        << NetworkId(network);
    return;
  }
  NET_LOG(EVENT) << "NetworkingPrivate.NetworkPropertiesUpdated: "
                 << NetworkId(network);
  auto args(api::networking_private::OnNetworksChanged::Create(
      std::vector<std::string>(1, network->guid())));
  std::unique_ptr<Event> extension_event(new Event(
      events::NETWORKING_PRIVATE_ON_NETWORKS_CHANGED,
      api::networking_private::OnNetworksChanged::kEventName, std::move(args)));
  event_router->BroadcastEvent(std::move(extension_event));
}

void NetworkingPrivateEventRouterImpl::DevicePropertiesUpdated(
    const DeviceState* device) {
  // networkingPrivate uses a single event for device changes.
  DeviceListChanged();

  // DeviceState changes may affect Cellular networks.
  if (device->type() != shill::kTypeCellular) {
    return;
  }

  NetworkStateHandler::NetworkStateList cellular_networks;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      ash::NetworkTypePattern::Cellular(), false /* configured_only */,
      true /* visible_only */, -1 /* default limit */, &cellular_networks);
  for (const NetworkState* network : cellular_networks) {
    NetworkPropertiesUpdated(network);
  }
}

void NetworkingPrivateEventRouterImpl::ScanCompleted(
    const DeviceState* device) {
  // We include the scanning state for Cellular networks, so notify the UI when
  // a scan completes.
  if (ash::NetworkTypePattern::Wireless().MatchesType(device->type())) {
    DevicePropertiesUpdated(device);
  }
}

void NetworkingPrivateEventRouterImpl::OnCertificatesChanged() {
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          api::networking_private::OnCertificateListsChanged::kEventName)) {
    NET_LOG(EVENT) << "NetworkingPrivate.OnCertificatesChanged: No Listeners";
    return;
  }
  NET_LOG(EVENT) << "NetworkingPrivate.OnCertificatesChanged";

  auto args(api::networking_private::OnCertificateListsChanged::Create());
  std::unique_ptr<Event> extension_event(
      new Event(events::NETWORKING_PRIVATE_ON_CERTIFICATE_LISTS_CHANGED,
                api::networking_private::OnCertificateListsChanged::kEventName,
                std::move(args)));
  event_router->BroadcastEvent(std::move(extension_event));
}

void NetworkingPrivateEventRouterImpl::PortalStateChanged(
    const NetworkState* network,
    NetworkState::PortalState portal_state) {
  const std::string guid = network ? network->guid() : std::string();

  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          api::networking_private::OnPortalDetectionCompleted::kEventName)) {
    NET_LOG(EVENT)
        << "NetworkingPrivate.OnPortalDetectionCompleted: No Listeners: "
        << (network ? NetworkId(network) : "");
    return;
  }
  NET_LOG(EVENT) << "NetworkingPrivate.OnPortalDetectionCompleted: "
                 << (network ? NetworkId(network) : "");

  auto args(api::networking_private::OnPortalDetectionCompleted::Create(
      guid, GetCaptivePortalStatus(network)));
  std::unique_ptr<Event> extension_event(
      new Event(events::NETWORKING_PRIVATE_ON_PORTAL_DETECTION_COMPLETED,
                api::networking_private::OnPortalDetectionCompleted::kEventName,
                std::move(args)));
  event_router->BroadcastEvent(std::move(extension_event));
}

std::unique_ptr<NetworkingPrivateEventRouter>
NetworkingPrivateEventRouter::Create(content::BrowserContext* context) {
  return std::make_unique<NetworkingPrivateEventRouterImpl>(context);
}

}  // namespace extensions
