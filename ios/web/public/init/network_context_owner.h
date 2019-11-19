// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_INIT_NETWORK_CONTEXT_OWNER_H_
#define IOS_WEB_PUBLIC_INIT_NETWORK_CONTEXT_OWNER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "ios/web/public/thread/web_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_getter_observer.h"
#include "services/network/network_context.h"

namespace web {

// Class that owns a NetworkContext wrapping the URLRequestContext.
// This allows using the URLLoaderFactory and NetworkContext APIs
// while still issuing requests with a URLRequestContext desired,
// whether a standard //net one, or one created by a BrowserState subclass.
//
// Created on the UI thread on first use, so the UI-thread living objects
// like BrowserState can own the NetworkContextOwner.  A task is then posted to
// the IO thread to create the NetworkContext itself, which has to live on the
// IO thread, since that's where the URLRequestContext lives.  Destroyed on the
// IO thread during shutdown, to ensure the NetworkContext is destroyed on the
// right thread.
class NetworkContextOwner : public net::URLRequestContextGetterObserver {
 public:
  // This initiates creation of the NetworkContext object on I/O thread and
  // connects the pipe in |network_context_client| to it.
  NetworkContextOwner(
      net::URLRequestContextGetter* request_context,
      const std::vector<std::string>& cors_exempt_header_list,
      mojo::Remote<network::mojom::NetworkContext>* network_context_client);

  ~NetworkContextOwner() override;

  // net::URLRequestContextGetterObserver implementation:
  void OnContextShuttingDown() override;

 private:
  void InitializeOnIOThread(
      const std::vector<std::string> cors_exempt_header_list,
      mojo::PendingReceiver<network::mojom::NetworkContext>
          network_context_receiver);

  scoped_refptr<net::URLRequestContextGetter> request_context_;
  std::unique_ptr<network::NetworkContext> network_context_;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_INIT_NETWORK_CONTEXT_OWNER_H_
