// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_H_

#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// This is an event router that will broadcast chrome.networkingPrivate
// events when there are listeners to them in the Javascript side.
//
// On Ash-chrome it means forwarding events from the NetworkStateHandler.
// Elsewhere (including Lacros-chrome) it means forwarding events from the
// NetworkingPrivateDelegate.
class NetworkingPrivateEventRouter : public KeyedService,
                                     public EventRouter::Observer {
 public:
  NetworkingPrivateEventRouter(const NetworkingPrivateEventRouter&) = delete;
  NetworkingPrivateEventRouter& operator=(const NetworkingPrivateEventRouter&) =
      delete;

  static std::unique_ptr<NetworkingPrivateEventRouter> Create(
      content::BrowserContext* browser_context);

 protected:
  NetworkingPrivateEventRouter() = default;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_H_
