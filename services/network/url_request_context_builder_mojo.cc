// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_request_context_builder_mojo.h"

#include "base/check.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/pac_file_fetcher_impl.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "services/network/network_context.h"
#include "services/network/proxy_service_mojo.h"
#include "services/network/public/cpp/features.h"
#if BUILDFLAG(IS_WIN)
#include "net/proxy_resolution/win/dhcp_pac_file_fetcher_win.h"
#include "net/proxy_resolution/win/windows_system_proxy_resolution_service.h"
#include "services/network/windows_system_proxy_resolver_mojo.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "services/network/dhcp_pac_file_fetcher_mojo.h"
#endif

namespace network {

URLRequestContextBuilderMojo::URLRequestContextBuilderMojo() = default;

URLRequestContextBuilderMojo::~URLRequestContextBuilderMojo() = default;

void URLRequestContextBuilderMojo::SetMojoProxyResolverFactory(
    mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
        mojo_proxy_resolver_factory) {
  mojo_proxy_resolver_factory_ = std::move(mojo_proxy_resolver_factory);
}

#if BUILDFLAG(IS_WIN)
void URLRequestContextBuilderMojo::SetMojoWindowsSystemProxyResolver(
    mojo::PendingRemote<proxy_resolver_win::mojom::WindowsSystemProxyResolver>
        mojo_windows_system_proxy_resolver) {
  mojo_windows_system_proxy_resolver_ =
      std::move(mojo_windows_system_proxy_resolver);
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
void URLRequestContextBuilderMojo::SetDhcpWpadUrlClient(
    mojo::PendingRemote<network::mojom::DhcpWpadUrlClient>
        dhcp_wpad_url_client) {
  dhcp_wpad_url_client_ = std::move(dhcp_wpad_url_client);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::unique_ptr<net::DhcpPacFileFetcher>
URLRequestContextBuilderMojo::CreateDhcpPacFileFetcher(
    net::URLRequestContext* context) {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<net::DhcpPacFileFetcherWin>(context);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<DhcpPacFileFetcherMojo>(
      context, std::move(dhcp_wpad_url_client_));
#else
  return std::make_unique<net::DoNothingDhcpPacFileFetcher>();
#endif
}

std::unique_ptr<net::ProxyResolutionService>
URLRequestContextBuilderMojo::CreateProxyResolutionService(
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    net::URLRequestContext* url_request_context,
    net::HostResolver* host_resolver,
    net::NetworkDelegate* network_delegate,
    net::NetLog* net_log,
    bool pac_quick_check_enabled) {
  DCHECK(url_request_context);
  DCHECK(host_resolver);

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40111093): Support both ProxyResolutionService
  // implementations so that they can be swapped around at runtime based on
  // proxy config.
  if (mojo_windows_system_proxy_resolver_) {
    std::unique_ptr<net::ProxyResolutionService> proxy_resolution_service =
        net::WindowsSystemProxyResolutionService::Create(
            std::make_unique<WindowsSystemProxyResolverMojo>(
                std::move(mojo_windows_system_proxy_resolver_)),
            net_log);
    if (proxy_resolution_service)
      return proxy_resolution_service;
  }
#endif

  if (mojo_proxy_resolver_factory_) {
    std::unique_ptr<net::DhcpPacFileFetcher> dhcp_pac_file_fetcher =
        CreateDhcpPacFileFetcher(url_request_context);

    std::unique_ptr<net::PacFileFetcherImpl> pac_file_fetcher;
    pac_file_fetcher = net::PacFileFetcherImpl::Create(url_request_context);
    return CreateConfiguredProxyResolutionServiceUsingMojoFactory(
        std::move(mojo_proxy_resolver_factory_),
        std::move(proxy_config_service), std::move(pac_file_fetcher),
        std::move(dhcp_pac_file_fetcher), host_resolver, net_log,
        pac_quick_check_enabled, network_delegate);
  }

  return net::URLRequestContextBuilder::CreateProxyResolutionService(
      std::move(proxy_config_service), url_request_context, host_resolver,
      network_delegate, net_log, pac_quick_check_enabled);
}

}  // namespace network
