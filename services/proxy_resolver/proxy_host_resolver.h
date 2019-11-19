// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PROXY_RESOLVER_PROXY_HOST_RESOLVER_H_
#define SERVICES_PROXY_RESOLVER_PROXY_HOST_RESOLVER_H_

#include <memory>
#include <string>
#include <vector>

#include "net/base/completion_once_callback.h"
#include "net/base/ip_address.h"
#include "net/proxy_resolution/proxy_resolve_dns_operation.h"

namespace net {
class NetworkIsolationKey;
}  // namespace net

namespace proxy_resolver {

// Interface for a limited (compared to the standard HostResolver) host resolver
// used just for proxy resolution.
class ProxyHostResolver {
 public:
  virtual ~ProxyHostResolver() {}

  class Request {
   public:
    virtual ~Request() {}
    virtual int Start(net::CompletionOnceCallback callback) = 0;
    virtual const std::vector<net::IPAddress>& GetResults() const = 0;
  };

  virtual std::unique_ptr<Request> CreateRequest(
      const std::string& hostname,
      net::ProxyResolveDnsOperation operation,
      const net::NetworkIsolationKey& network_isolation_key) = 0;
};

}  // namespace proxy_resolver

#endif  // SERVICES_PROXY_RESOLVER_PROXY_HOST_RESOLVER_H_
