// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PROXY_SERVICE_MOJO_H_
#define SERVICES_NETWORK_PROXY_SERVICE_MOJO_H_

#include <memory>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/proxy_resolution/dhcp_pac_file_fetcher.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"

namespace net {
class ConfiguredProxyResolutionService;
class HostResolver;
class NetLog;
class NetworkDelegate;
class ProxyConfigService;
class PacFileFetcher;
}  // namespace net

namespace network {

// Creates a proxy resolution service that uses |mojo_proxy_factory| to create
// and connect to a Mojo service for evaluating PAC files
// (ProxyResolverFactory). The proxy service observes |proxy_config_service| to
// notice when the proxy settings change.
//
// |pac_file_fetcher| specifies the dependency to use for downloading
// any PAC scripts.
//
// |dhcp_pac_file_fetcher| specifies the dependency to use for attempting
// to retrieve the most appropriate PAC script configured in DHCP.
//
// |host_resolver| points to the host resolving dependency the PAC script
// should use for any DNS queries. It must remain valid throughout the
// lifetime of the ConfiguredProxyResolutionService.
COMPONENT_EXPORT(NETWORK_SERVICE)
std::unique_ptr<net::ConfiguredProxyResolutionService>
CreateConfiguredProxyResolutionServiceUsingMojoFactory(
    mojo::PendingRemote<proxy_resolver::mojom::ProxyResolverFactory>
        mojo_proxy_factory,
    std::unique_ptr<net::ProxyConfigService> proxy_config_service,
    std::unique_ptr<net::PacFileFetcher> pac_file_fetcher,
    std::unique_ptr<net::DhcpPacFileFetcher> dhcp_pac_file_fetcher,
    net::HostResolver* host_resolver,
    net::NetLog* net_log,
    bool pac_quick_check_enabled,
    net::NetworkDelegate* network_delegate);

}  // namespace network

#endif  // SERVICES_NETWORK_PROXY_SERVICE_MOJO_H_
