// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_
#define SERVICES_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/url_request_context_owner.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

#if defined(OS_CHROMEOS)
#include "services/network/public/mojom/dhcp_wpad_url_client.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace net {
class HostResolver;
class NetLog;
class NetworkDelegate;
class ProxyResolutionService;
class URLRequestContext;
}  // namespace net

namespace network {
// Specialization of URLRequestContextBuilder that can create a
// ProxyResolutionService that uses a Mojo ProxyResolver. The consumer is
// responsible for providing the proxy_resolver::mojom::ProxyResolverFactory.
// If a ProxyResolutionService is set directly via the URLRequestContextBuilder
// API, it will be used instead.
class COMPONENT_EXPORT(NETWORK_SERVICE) URLRequestContextBuilderMojo
    : public net::URLRequestContextBuilder {
 public:
  URLRequestContextBuilderMojo();
  ~URLRequestContextBuilderMojo() override;

  // Sets Mojo factory used to create ProxyResolvers. If not set, falls back to
  // URLRequestContext's default behavior.
  void SetMojoProxyResolverFactory(
      proxy_resolver::mojom::ProxyResolverFactoryPtr
          mojo_proxy_resolver_factory);

#if defined(OS_CHROMEOS)
  void SetDhcpWpadUrlClient(
      network::mojom::DhcpWpadUrlClientPtr dhcp_wpad_url_client);
#endif  // defined(OS_CHROMEOS)

 private:
  std::unique_ptr<net::ProxyResolutionService> CreateProxyResolutionService(
      std::unique_ptr<net::ProxyConfigService> proxy_config_service,
      net::URLRequestContext* url_request_context,
      net::HostResolver* host_resolver,
      net::NetworkDelegate* network_delegate,
      net::NetLog* net_log) override;

  std::unique_ptr<net::DhcpPacFileFetcher> CreateDhcpPacFileFetcher(
      net::URLRequestContext* context);

#if defined(OS_CHROMEOS)
  // If set, handles calls to get the PAC script URL from the browser process.
  // Only used if |mojo_proxy_resolver_factory_| is set.
  network::mojom::DhcpWpadUrlClientPtr dhcp_wpad_url_client_;
#endif  // defined(OS_CHROMEOS)

  proxy_resolver::mojom::ProxyResolverFactoryPtr mojo_proxy_resolver_factory_;
  DISALLOW_COPY_AND_ASSIGN(URLRequestContextBuilderMojo);
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_
