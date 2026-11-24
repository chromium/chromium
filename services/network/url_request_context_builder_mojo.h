// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_
#define SERVICES_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/url_request_context_owner.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "services/proxy_resolver/public/mojom/system_proxy_resolver.mojom.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
#include "services/network/public/mojom/dhcp_wpad_url_client.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace net {
class DhcpPacFileFetcher;
class HostResolver;
class NetLog;
class NetworkDelegate;
class ProxyResolutionService;
class URLRequestContext;
}  // namespace net

namespace network {
// Builds a configured proxy service using a Mojo ProxyResolverFactory and/or
// SystemProxyResolver. On Windows, macOS, and Linux, the system resolver is an
// optional native backend of the configured service. A proxy service set
// directly through URLRequestContextBuilder takes precedence.
class COMPONENT_EXPORT(NETWORK_SERVICE) URLRequestContextBuilderMojo
    : public net::URLRequestContextBuilder {
 public:
  URLRequestContextBuilderMojo();

  URLRequestContextBuilderMojo(const URLRequestContextBuilderMojo&) = delete;
  URLRequestContextBuilderMojo& operator=(const URLRequestContextBuilderMojo&) =
      delete;

  ~URLRequestContextBuilderMojo() override;

  // Sets Mojo factory used to create ProxyResolvers. If not set, falls back to
  // URLRequestContext's default behavior.
  void SetMojoProxyResolverFactory(
      mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
          mojo_proxy_resolver_factory);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  void SetMojoSystemProxyResolver(
      mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
          mojo_system_proxy_resolver);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
  void SetDhcpWpadUrlClient(
      mojo::PendingRemote<network::mojom::DhcpWpadUrlClient>
          dhcp_wpad_url_client);
#endif  // BUILDFLAG(IS_CHROMEOS)

 private:
  std::unique_ptr<net::ProxyResolutionService> CreateProxyResolutionService(
      std::unique_ptr<net::ProxyConfigService> proxy_config_service,
      net::URLRequestContext* url_request_context,
      net::HostResolver* host_resolver,
      net::NetworkDelegate* network_delegate,
      net::NetLog* net_log,
      bool pac_quick_check_enabled) override;

  std::unique_ptr<net::DhcpPacFileFetcher> CreateDhcpPacFileFetcher(
      net::URLRequestContext* context);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Creates a platform-specific system proxy resolution service (Windows,
  // macOS, or Linux) using `mojo_system_proxy_resolver_`. Returns nullptr if
  // the resolver is unset or unsupported on the current platform version.
  std::unique_ptr<net::ProxyResolutionService>
  CreateSystemProxyResolutionService(net::NetLog* net_log);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_CHROMEOS)
  // If set, handles calls to get the PAC script URL from the browser process.
  // Only used if |mojo_proxy_resolver_factory_| is set.
  mojo::PendingRemote<network::mojom::DhcpWpadUrlClient> dhcp_wpad_url_client_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
      mojo_proxy_resolver_factory_;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  mojo::PendingRemote<proxy_resolver::mojom::SystemProxyResolver>
      mojo_system_proxy_resolver_;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_
