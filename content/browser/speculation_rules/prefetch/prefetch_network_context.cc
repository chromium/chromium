// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_network_context.h"

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/speculation_rules/prefetch/prefetch_network_context_client.h"
#include "content/browser/speculation_rules/prefetch/prefetch_proxy_configurator.h"
#include "content/browser/speculation_rules/prefetch/prefetch_service.h"
#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/isolation_info.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

PrefetchNetworkContext::PrefetchNetworkContext(
    PrefetchService* prefetch_service,
    const PrefetchType& prefetch_type)
    : prefetch_service_(prefetch_service), prefetch_type_(prefetch_type) {}

PrefetchNetworkContext::~PrefetchNetworkContext() = default;

network::mojom::NetworkContext* PrefetchNetworkContext::GetNetworkContext()
    const {
  DCHECK(network_context_);
  return network_context_.get();
}

network::mojom::URLLoaderFactory*
PrefetchNetworkContext::GetURLLoaderFactory() {
  if (!url_loader_factory_) {
    if (prefetch_type_.IsIsolatedNetworkContextRequired()) {
      CreateIsolatedURLLoaderFactory();
      DCHECK(network_context_);
    } else {
      // TODO(crbug.com/1278103): Use
      // RenderFrameHost::CreateNetworkServiceDefaultFactory if possible.
      url_loader_factory_ = prefetch_service_->GetBrowserContext()
                                ->GetDefaultStoragePartition()
                                ->GetURLLoaderFactoryForBrowserProcess();
    }
  }
  DCHECK(url_loader_factory_);
  return url_loader_factory_.get();
}

network::mojom::CookieManager* PrefetchNetworkContext::GetCookieManager() {
  DCHECK(prefetch_type_.IsIsolatedNetworkContextRequired());
  DCHECK(network_context_);
  if (!cookie_manager_)
    network_context_->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());

  return cookie_manager_.get();
}

void PrefetchNetworkContext::CloseIdleConnections() {
  if (network_context_)
    network_context_->CloseIdleConnections(base::DoNothing());
}

void PrefetchNetworkContext::CreateIsolatedURLLoaderFactory() {
  DCHECK(prefetch_type_.IsIsolatedNetworkContextRequired());

  network_context_.reset();
  url_loader_factory_.reset();

  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->user_agent = content::GetReducedUserAgent(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseMobileUserAgent),
      "");
  context_params->cert_verifier_params = GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  context_params->cors_exempt_header_list = {kCorsExemptPurposeHeaderName};
  context_params->cookie_manager_params =
      network::mojom::CookieManagerParams::New();

  // TODO(https://crbug.com/1299059): Get major version of Chrome when
  // constructing the user agent. This will require a delegate.
  // TODO(https://crbug.com/1299059): Add the accept languages stored in prefs.
  // This will require a delegate.

  context_params->http_cache_enabled = true;
  DCHECK(!context_params->http_cache_directory);

  if (prefetch_type_.IsProxyRequired()) {
    PrefetchProxyConfigurator* prefetch_proxy_configurator =
        prefetch_service_->GetPrefetchProxyConfigurator();

    context_params->initial_custom_proxy_config =
        prefetch_proxy_configurator->CreateCustomProxyConfig();
    context_params->custom_proxy_connection_observer_remote =
        prefetch_proxy_configurator->NewProxyConnectionObserverRemote();

    // Register a client config receiver so that updates to the set of proxy
    // hosts or proxy headers will be updated.
    mojo::Remote<network::mojom::CustomProxyConfigClient> config_client;
    context_params->custom_proxy_config_client_receiver =
        config_client.BindNewPipeAndPassReceiver();
    prefetch_proxy_configurator->AddCustomProxyConfigClient(
        std::move(config_client), base::DoNothing());
  }

  // Explicitly disallow network service features which could cause a privacy
  // leak.
  context_params->enable_certificate_reporting = false;
  context_params->enable_expect_ct_reporting = false;
  context_params->enable_domain_reliability = false;

  CreateNetworkContextInNetworkService(
      network_context_.BindNewPipeAndPassReceiver(), std::move(context_params));

  if (prefetch_type_.IsProxyRequired()) {
    // Configure a context client to ensure Web Reports and other privacy leak
    // surfaces won't be enabled.
    mojo::PendingRemote<network::mojom::NetworkContextClient> client_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<PrefetchNetworkContextClient>(),
        client_remote.InitWithNewPipeAndPassReceiver());
    network_context_->SetClient(std::move(client_remote));
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> isolated_factory_remote;

  CreateNewURLLoaderFactory(
      isolated_factory_remote.InitWithNewPipeAndPassReceiver(), absl::nullopt);
  url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
          std::move(isolated_factory_remote)));
}

void PrefetchNetworkContext::CreateNewURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    absl::optional<net::IsolationInfo> isolation_info) {
  DCHECK(prefetch_type_.IsIsolatedNetworkContextRequired());
  DCHECK(network_context_);

  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = network::mojom::kBrowserProcessId;
  factory_params->is_trusted = true;
  factory_params->is_corb_enabled = false;
  if (isolation_info) {
    factory_params->isolation_info = *isolation_info;
  }

  network_context_->CreateURLLoaderFactory(std::move(pending_receiver),
                                           std::move(factory_params));
}

}  // namespace content