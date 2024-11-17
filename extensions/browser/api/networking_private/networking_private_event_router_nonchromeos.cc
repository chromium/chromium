// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "extensions/browser/api/networking_private/networking_private_event_router.h"

#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_observer.h"
#include "extensions/common/api/networking_private.h"

namespace extensions {

namespace {

constexpr const char* kEventNames[] = {
    api::networking_private::OnNetworksChanged::kEventName,
    api::networking_private::OnNetworkListChanged::kEventName,
};

}  // namespace

class NetworkingPrivateEventRouterImpl
    : public NetworkingPrivateEventRouter,
      public NetworkingPrivateDelegateObserver {
 public:
  explicit NetworkingPrivateEventRouterImpl(
      content::BrowserContext* browser_context);

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

  // NetworkingPrivateDelegateObserver overrides:
  void OnNetworksChangedEvent(
      const std::vector<std::string>& network_guids) override;
  void OnNetworkListChangedEvent(
      const std::vector<std::string>& network_guids) override;
  void OnDeviceStateListChanged() override;
  void OnPortalDetectionCompleted(
      std::string networkGuid,
      api::networking_private::CaptivePortalStatus status) override;
  void OnCertificateListsChanged() override;

 private:
  // Decide if we should listen for network changes or not. If there are any
  // JavaScript listeners registered for the onNetworkChanged event, then we
  // want to register for change notification from the network state handler.
  // Otherwise, we want to unregister and not be listening to network changes.
  void StartOrStopListeningForNetworkChanges();

  raw_ptr<content::BrowserContext> browser_context_;
  bool listening_ = false;
};

NetworkingPrivateEventRouterImpl::NetworkingPrivateEventRouterImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all profile services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  for (const char* name : kEventNames) {
    event_router->RegisterObserver(this, name);
  }

  StartOrStopListeningForNetworkChanges();
}

NetworkingPrivateEventRouterImpl::~NetworkingPrivateEventRouterImpl() {
  DCHECK(!listening_);
}

void NetworkingPrivateEventRouterImpl::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all profile services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->UnregisterObserver(this);
  }

  if (!listening_) {
    return;
  }
  listening_ = false;
  NetworkingPrivateDelegate* delegate =
      NetworkingPrivateDelegateFactory::GetForBrowserContext(browser_context_);
  if (delegate) {
    delegate->RemoveObserver(this);
  }
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
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  bool should_listen = false;

  for (const char* name : kEventNames) {
    if (event_router->HasEventListener(name)) {
      should_listen = true;
      break;
    }
  }

  if (should_listen && !listening_) {
    NetworkingPrivateDelegate* delegate =
        NetworkingPrivateDelegateFactory::GetForBrowserContext(
            browser_context_);
    if (delegate) {
      delegate->AddObserver(this);
    }
  }
  if (!should_listen && listening_) {
    NetworkingPrivateDelegate* delegate =
        NetworkingPrivateDelegateFactory::GetForBrowserContext(
            browser_context_);
    if (delegate) {
      delegate->RemoveObserver(this);
    }
  }

  listening_ = should_listen;
}

void NetworkingPrivateEventRouterImpl::OnNetworksChangedEvent(
    const std::vector<std::string>& network_guids) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }
  auto args(api::networking_private::OnNetworksChanged::Create(network_guids));
  std::unique_ptr<Event> netchanged_event(new Event(
      events::NETWORKING_PRIVATE_ON_NETWORKS_CHANGED,
      api::networking_private::OnNetworksChanged::kEventName, std::move(args)));
  event_router->BroadcastEvent(std::move(netchanged_event));
}

void NetworkingPrivateEventRouterImpl::OnNetworkListChangedEvent(
    const std::vector<std::string>& network_guids) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }
  auto args(
      api::networking_private::OnNetworkListChanged::Create(network_guids));
  std::unique_ptr<Event> netlistchanged_event(
      new Event(events::NETWORKING_PRIVATE_ON_NETWORK_LIST_CHANGED,
                api::networking_private::OnNetworkListChanged::kEventName,
                std::move(args)));
  event_router->BroadcastEvent(std::move(netlistchanged_event));
}

void NetworkingPrivateEventRouterImpl::OnDeviceStateListChanged() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  auto args(api::networking_private::OnDeviceStateListChanged::Create());
  auto extension_event = std::make_unique<Event>(
      events::NETWORKING_PRIVATE_ON_DEVICE_STATE_LIST_CHANGED,
      api::networking_private::OnDeviceStateListChanged::kEventName,
      std::move(args));
  event_router->BroadcastEvent(std::move(extension_event));
}

void NetworkingPrivateEventRouterImpl::OnPortalDetectionCompleted(
    std::string guid,
    api::networking_private::CaptivePortalStatus status) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  auto args(api::networking_private::OnPortalDetectionCompleted::Create(
      guid, status));
  auto extension_event = std::make_unique<Event>(
      events::NETWORKING_PRIVATE_ON_PORTAL_DETECTION_COMPLETED,
      api::networking_private::OnPortalDetectionCompleted::kEventName,
      std::move(args));
  event_router->BroadcastEvent(std::move(extension_event));
}

void NetworkingPrivateEventRouterImpl::OnCertificateListsChanged() {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router) {
    return;
  }

  auto args(api::networking_private::OnCertificateListsChanged::Create());
  auto extension_event = std::make_unique<Event>(
      events::NETWORKING_PRIVATE_ON_CERTIFICATE_LISTS_CHANGED,
      api::networking_private::OnCertificateListsChanged::kEventName,
      std::move(args));
  event_router->BroadcastEvent(std::move(extension_event));
}

std::unique_ptr<NetworkingPrivateEventRouter>
NetworkingPrivateEventRouter::Create(content::BrowserContext* browser_context) {
  return std::make_unique<NetworkingPrivateEventRouterImpl>(browser_context);
}

}  // namespace extensions
