// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MAC_SYSTEM_PROXY_RESOLVER_MOJO_H_
#define SERVICES_NETWORK_MAC_SYSTEM_PROXY_RESOLVER_MOJO_H_

#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/proxy_resolution/mac/mac_system_proxy_resolver.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "url/gurl.h"

namespace net {
class MacSystemProxyResolutionRequest;
}  // namespace net

namespace network {

// This is the concrete implementation of net::MacSystemProxyResolver that
// connects to a Mojo service to actually do proxy resolution. It contains no
// business logic, only passing the request along to the service.
class COMPONENT_EXPORT(NETWORK_SERVICE) MacSystemProxyResolverMojo final
    : public net::MacSystemProxyResolver {
 public:
  explicit MacSystemProxyResolverMojo(
      mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
          mojo_mac_system_proxy_resolver);
  MacSystemProxyResolverMojo(const MacSystemProxyResolverMojo&) = delete;
  MacSystemProxyResolverMojo& operator=(const MacSystemProxyResolverMojo&) =
      delete;
  ~MacSystemProxyResolverMojo() override;

  // net::MacSystemProxyResolver implementation
  std::unique_ptr<net::MacSystemProxyResolver::Request> GetProxyForUrl(
      const GURL& url,
      net::MacSystemProxyResolutionRequest* callback_target) override;

 private:
  class RequestImpl;

  mojo::Remote<proxy_resolver::mojom::SystemProxyResolver>
      mojo_mac_system_proxy_resolver_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_MAC_SYSTEM_PROXY_RESOLVER_MOJO_H_
