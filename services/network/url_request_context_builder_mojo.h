// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_
#define SERVICES_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/url_request_context_owner.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/network/public/mojom/dhcp_wpad_url_client.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
#include "services/proxy_resolver_win/public/mojom/proxy_resolver_win.mojom.h"
#endif

namespace net {
class DhcpPacFileFetcher;
class HostResolver;
class NetLog;
class NetworkDelegate;
class ProxyResolutionService;
class URLRequestContext;
}  // namespace net

namespace network {
// Specialization of URLRequestContextBuilder that can create one or more
// ProxyResolutionServices that use Mojo. This can be a
// ConfiguredProxyResolutionService that uses a Mojo ProxyResolver or a
// WindowsSystemProxyResolutionService that may mojo all proxy resolutions to a
// utility process if enabled. The consumer is responsible for providing either
// the proxy_resolver::mojom::ProxyResolverFactory or
// proxy_resolver_win::mojom::WindowsSystemProxyResolver respectively. If a
// ProxyResolutionService is set directly via the URLRequestContextBuilder API,
// it will be used instead either of the ProxyResolutionService implementations
// mentioned here.
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

#if BUILDFLAG(IS_WIN)
  void SetMojoWindowsSystemProxyResolver(
      mojo::PendingRemote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
          mojo_windows_system_proxy_resolver);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetDhcpWpadUrlClient(
      mojo::PendingRemote<network::mojom::DhcpWpadUrlClient>
          dhcp_wpad_url_client);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If set, handles calls to get the PAC script URL from the browser process.
  // Only used if |mojo_proxy_resolver_factory_| is set.
  mojo::PendingRemote<network::mojom::DhcpWpadUrlClient> dhcp_wpad_url_client_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
      mojo_proxy_resolver_factory_;

#if BUILDFLAG(IS_WIN)
  mojo::PendingRemote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
      mojo_windows_system_proxy_resolver_;
#endif
};

}  // namespace network

#endif  // SERVICES_NETWORK_URL_REQUEST_CONTEXT_BUILDER_MOJO_H_
