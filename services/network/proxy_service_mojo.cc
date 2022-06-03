// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/proxy_service_mojo.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "net/proxy_resolution/network_delegate_error_observer.h"
#include "net/proxy_resolution/proxy_resolver_factory.h"
#include "services/network/proxy_resolver_factory_mojo.h"

namespace network {

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
    net::NetworkDelegate* network_delegate) {
  DCHECK(proxy_config_service);
  DCHECK(pac_file_fetcher);
  DCHECK(dhcp_pac_file_fetcher);
  DCHECK(host_resolver);

  std::unique_ptr<net::ConfiguredProxyResolutionService>
      proxy_resolution_service(new net::ConfiguredProxyResolutionService(
          std::move(proxy_config_service),
          std::make_unique<ProxyResolverFactoryMojo>(
              std::move(mojo_proxy_factory), host_resolver,
              base::BindRepeating(&net::NetworkDelegateErrorObserver::Create,
                                  network_delegate,
                                  base::ThreadTaskRunnerHandle::Get()),
              net_log),
          net_log, pac_quick_check_enabled));

  // Configure fetchers to use for PAC script downloads and auto-detect.
  proxy_resolution_service->SetPacFileFetchers(
      std::move(pac_file_fetcher), std::move(dhcp_pac_file_fetcher));

  return proxy_resolution_service;
}

}  // namespace network
