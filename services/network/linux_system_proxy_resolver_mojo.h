// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_LINUX_SYSTEM_PROXY_RESOLVER_MOJO_H_
#define SERVICES_NETWORK_LINUX_SYSTEM_PROXY_RESOLVER_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/proxy_resolution/linux/linux_system_proxy_resolver.h"
#include "services/proxy_resolver/public/mojom/system_proxy_resolver.mojom.h"
#include "url/gurl.h"

namespace network {

// This is the concrete implementation of `net::LinuxSystemProxyResolver` that
// connects to a Mojo service (hosted in the browser process) to actually do
// proxy resolution. It contains no business logic, only passing the request
// along to the service.
class COMPONENT_EXPORT(NETWORK_SERVICE) LinuxSystemProxyResolverMojo final
    : public net::LinuxSystemProxyResolver {
 public:
  explicit LinuxSystemProxyResolverMojo(
      mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
          mojo_linux_system_proxy_resolver);
  LinuxSystemProxyResolverMojo(const LinuxSystemProxyResolverMojo&) = delete;
  LinuxSystemProxyResolverMojo& operator=(const LinuxSystemProxyResolverMojo&) =
      delete;
  ~LinuxSystemProxyResolverMojo() override;

  // `net::LinuxSystemProxyResolver` implementation:
  std::unique_ptr<net::LinuxSystemProxyResolver::Request> GetProxyForUrl(
      const GURL& url,
      ResultCallback callback) override;

 private:
  class RequestImpl;

  mojo::Remote<proxy_resolver::mojom::SystemProxyResolver>
      mojo_linux_system_proxy_resolver_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_LINUX_SYSTEM_PROXY_RESOLVER_MOJO_H_
