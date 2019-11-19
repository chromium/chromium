// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_request_context_builder_mojo.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "net/proxy_resolution/pac_file_fetcher_impl.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "services/network/network_context.h"
#include "services/network/proxy_service_mojo.h"
#include "services/network/public/cpp/features.h"
#if defined(OS_WIN)
#include "net/proxy_resolution/dhcp_pac_file_fetcher_win.h"
#elif defined(OS_CHROMEOS)
#include "services/network/dhcp_pac_file_fetcher_mojo.h"
#endif

namespace network {

URLRequestContextBuilderMojo::URLRequestContextBuilderMojo() = default;

URLRequestContextBuilderMojo::~URLRequestContextBuilderMojo() = default;

void URLRequestContextBuilderMojo::SetMojoProxyResolverFactory(
    proxy_resolver::mojom::ProxyResolverFactoryPtr
        mojo_proxy_resolver_factory) {
  mojo_proxy_resolver_factory_ = std::move(mojo_proxy_resolver_factory);
}

#if defined(OS_CHROMEOS)
void URLRequestContextBuilderMojo::SetDhcpWpadUrlClient(
    network::mojom::DhcpWpadUrlClientPtr dhcp_wpad_url_client) {
  dhcp_wpad_url_client_ = std::move(dhcp_wpad_url_client);
}
#endif  // defined(OS_CHROMEOS)

std::unique_ptr<net::DhcpPacFileFetcher>
URLRequestContextBuilderMojo::CreateDhcpPacFileFetcher(
    net::URLRequestContext* context) {
#if defined(OS_WIN)
  return std::make_unique<net::DhcpPacFileFetcherWin>(context);
#elif defined(OS_CHROMEOS)
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
    net::NetLog* net_log) {
  DCHECK(url_request_context);
  DCHECK(host_resolver);

  if (mojo_proxy_resolver_factory_) {
    std::unique_ptr<net::DhcpPacFileFetcher> dhcp_pac_file_fetcher =
        CreateDhcpPacFileFetcher(url_request_context);

    std::unique_ptr<net::PacFileFetcherImpl> pac_file_fetcher;
    pac_file_fetcher = net::PacFileFetcherImpl::Create(url_request_context);
    return CreateProxyResolutionServiceUsingMojoFactory(
        std::move(mojo_proxy_resolver_factory_),
        std::move(proxy_config_service), std::move(pac_file_fetcher),
        std::move(dhcp_pac_file_fetcher), host_resolver, net_log,
        network_delegate);
  }

  return net::URLRequestContextBuilder::CreateProxyResolutionService(
      std::move(proxy_config_service), url_request_context, host_resolver,
      network_delegate, net_log);
}

}  // namespace network
