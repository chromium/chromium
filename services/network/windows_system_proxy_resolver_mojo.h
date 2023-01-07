// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WINDOWS_SYSTEM_PROXY_RESOLVER_MOJO_H_
#define SERVICES_NETWORK_WINDOWS_SYSTEM_PROXY_RESOLVER_MOJO_H_

#include <string>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolver.h"
#include "services/proxy_resolver_win/public/mojom/proxy_resolver_win.mojom.h"
#include "url/gurl.h"

namespace net {
class WindowsSystemProxyResolutionRequest;
}  // namespace net

namespace network {

// This is the concrete implementation of net::WindowsSystemProxyResolver that
// connects to a Mojo service to actually do proxy resolution. It contains no
// business logic, only passing the request along to the service.
class COMPONENT_EXPORT(NETWORK_SERVICE) WindowsSystemProxyResolverMojo final
    : public net::WindowsSystemProxyResolver {
 public:
  explicit WindowsSystemProxyResolverMojo(
      mojo::PendingRemote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
          mojo_windows_system_proxy_resolver);
  WindowsSystemProxyResolverMojo(const WindowsSystemProxyResolverMojo&) =
      delete;
  WindowsSystemProxyResolverMojo& operator=(
      const WindowsSystemProxyResolverMojo&) = delete;
  ~WindowsSystemProxyResolverMojo() override;

  // net::WindowsSystemProxyResolver implementation
  std::unique_ptr<net::WindowsSystemProxyResolver::Request> GetProxyForUrl(
      const GURL& url,
      net::WindowsSystemProxyResolutionRequest* callback_target) override;

 private:
  class RequestImpl;

  mojo::Remote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
      mojo_windows_system_proxy_resolver_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_WINDOWS_SYSTEM_PROXY_RESOLVER_MOJO_H_
