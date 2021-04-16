// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_event_router.h"

#include "base/macros.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_observer.h"
#include "extensions/common/api/networking_private.h"

namespace extensions {

// This is an event router that will observe listeners to |NetworksChanged| and
// |NetworkListChanged| events.
class NetworkingPrivateEventRouterImpl
    : public NetworkingPrivateEventRouter,
      public NetworkingPrivateDelegateObserver {
 public:
  explicit NetworkingPrivateEventRouterImpl(
      content::BrowserContext* browser_context);
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

 private:
  // Decide if we should listen for network changes or not. If there are any
  // JavaScript listeners registered for the onNetworkChanged event, then we
  // want to register for change notification from the network state handler.
  // Otherwise, we want to unregister and not be listening to network changes.
  void StartOrStopListeningForNetworkChanges();

  content::BrowserContext* browser_context_;
  bool listening_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateEventRouterImpl);
};

NetworkingPrivateEventRouterImpl::NetworkingPrivateEventRouterImpl(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context), listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all profile services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;
  event_router->RegisterObserver(
      this, api::networking_private::OnNetworksChanged::kEventName);
  event_router->RegisterObserver(
      this, api::networking_private::OnNetworkListChanged::kEventName);
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
  if (event_router)
    event_router->UnregisterObserver(this);

  if (!listening_)
    return;
  listening_ = false;
  NetworkingPrivateDelegate* delegate =
      NetworkingPrivateDelegateFactory::GetForBrowserContext(browser_context_);
  if (delegate)
    delegate->RemoveObserver(this);
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
  if (!event_router)
    return;

  bool should_listen =
      event_router->HasEventListener(
          api::networking_private::OnNetworksChanged::kEventName) ||
      event_router->HasEventListener(
          api::networking_private::OnNetworkListChanged::kEventName);

  if (should_listen && !listening_) {
    NetworkingPrivateDelegate* delegate =
        NetworkingPrivateDelegateFactory::GetForBrowserContext(
            browser_context_);
    if (delegate)
      delegate->AddObserver(this);
  }
  if (!should_listen && listening_) {
    NetworkingPrivateDelegate* delegate =
        NetworkingPrivateDelegateFactory::GetForBrowserContext(
            browser_context_);
    if (delegate)
      delegate->RemoveObserver(this);
  }

  listening_ = should_listen;
}

void NetworkingPrivateEventRouterImpl::OnNetworksChangedEvent(
    const std::vector<std::string>& network_guids) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;
  std::unique_ptr<base::ListValue> args(
      api::networking_private::OnNetworksChanged::Create(network_guids));
  std::unique_ptr<Event> netchanged_event(new Event(
      events::NETWORKING_PRIVATE_ON_NETWORKS_CHANGED,
      api::networking_private::OnNetworksChanged::kEventName, std::move(args)));
  event_router->BroadcastEvent(std::move(netchanged_event));
}

void NetworkingPrivateEventRouterImpl::OnNetworkListChangedEvent(
    const std::vector<std::string>& network_guids) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (!event_router)
    return;
  std::unique_ptr<base::ListValue> args(
      api::networking_private::OnNetworkListChanged::Create(network_guids));
  std::unique_ptr<Event> netlistchanged_event(
      new Event(events::NETWORKING_PRIVATE_ON_NETWORK_LIST_CHANGED,
                api::networking_private::OnNetworkListChanged::kEventName,
                std::move(args)));
  event_router->BroadcastEvent(std::move(netlistchanged_event));
}

NetworkingPrivateEventRouter* NetworkingPrivateEventRouter::Create(
    content::BrowserContext* browser_context) {
  return new NetworkingPrivateEventRouterImpl(browser_context);
}

}  // namespace extensions
