// Copyright 2013 The Chromium Authors. All rights reserved.
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

// This is an event router that will observe listeners to |NetworksChanged| and
// |NetworkListChanged| events. On ChromeOS it will forward these events
// from the NetworkStateHandler to the JavaScript Networking API.
class NetworkingPrivateEventRouter : public KeyedService,
                                     public EventRouter::Observer {
 public:
  NetworkingPrivateEventRouter(const NetworkingPrivateEventRouter&) = delete;
  NetworkingPrivateEventRouter& operator=(const NetworkingPrivateEventRouter&) =
      delete;

  static NetworkingPrivateEventRouter* Create(
      content::BrowserContext* browser_context);

 protected:
  NetworkingPrivateEventRouter() {}
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_EVENT_ROUTER_H_
