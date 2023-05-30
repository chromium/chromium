// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_network_context.h"

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "content/browser/preloading/prefetch/prefetch_network_context_client.h"
#include "content/browser/preloading/prefetch/prefetch_proxy_configurator.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
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
    bool use_isolated_network_context,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer_,
    const GlobalRenderFrameHostId& referring_render_frame_host_id)
    : prefetch_service_(prefetch_service),
      use_isolated_network_context_(use_isolated_network_context),
      prefetch_type_(prefetch_type),
      referrer_(referrer_),
      referring_render_frame_host_id_(referring_render_frame_host_id) {}

PrefetchNetworkContext::~PrefetchNetworkContext() = default;

network::mojom::NetworkContext* PrefetchNetworkContext::GetNetworkContext()
    const {
  CHECK(network_context_);
  return network_context_.get();
}

network::mojom::URLLoaderFactory*
PrefetchNetworkContext::GetURLLoaderFactory() {
  if (!url_loader_factory_) {
    if (use_isolated_network_context_) {
      CreateIsolatedURLLoaderFactory();
      CHECK(network_context_);
    } else {
      // Create new URL factory in the default network context.
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_factory_remote;
      CreateNewURLLoaderFactory(
          prefetch_service_->GetBrowserContext()
              ->GetDefaultStoragePartition()
              ->GetNetworkContext(),
          url_factory_remote.InitWithNewPipeAndPassReceiver(), absl::nullopt);
      url_loader_factory_ = network::SharedURLLoaderFactory::Create(
          std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
              std::move(url_factory_remote)));
    }
  }
  CHECK(url_loader_factory_);
  return url_loader_factory_.get();
}

network::mojom::CookieManager* PrefetchNetworkContext::GetCookieManager() {
  CHECK(use_isolated_network_context_);
  CHECK(network_context_);
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
  CHECK(use_isolated_network_context_);

  network_context_.reset();
  url_loader_factory_.reset();

  PrefetchServiceDelegate* delegate =
      prefetch_service_->GetPrefetchServiceDelegate();

  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->user_agent =
      GetReducedUserAgent(base::CommandLine::ForCurrentProcess()->HasSwitch(
                              switches::kUseMobileUserAgent),
                          delegate ? delegate->GetMajorVersionNumber() : "");
  context_params->cert_verifier_params = GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  context_params->cors_exempt_header_list = {kCorsExemptPurposeHeaderName};
  context_params->cookie_manager_params =
      network::mojom::CookieManagerParams::New();

  if (delegate) {
    context_params->accept_language = delegate->GetAcceptLanguageHeader();
  }

  context_params->http_cache_enabled = true;
  CHECK(!context_params->http_cache_directory);

  if (prefetch_type_.IsProxyRequiredWhenCrossOrigin() &&
      !prefetch_type_.IsProxyBypassedForTesting()) {
    PrefetchProxyConfigurator* prefetch_proxy_configurator =
        prefetch_service_->GetPrefetchProxyConfigurator();
    CHECK(prefetch_proxy_configurator);

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
  context_params->enable_domain_reliability = false;

  CreateNetworkContextInNetworkService(
      network_context_.BindNewPipeAndPassReceiver(), std::move(context_params));

  if (prefetch_type_.IsProxyRequiredWhenCrossOrigin() &&
      !prefetch_type_.IsProxyBypassedForTesting()) {
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
      network_context_.get(),
      isolated_factory_remote.InitWithNewPipeAndPassReceiver(), absl::nullopt);
  url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::make_unique<network::WrapperPendingSharedURLLoaderFactory>(
          std::move(isolated_factory_remote)));
}

void PrefetchNetworkContext::CreateNewURLLoaderFactory(
    network::mojom::NetworkContext* network_context,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver,
    absl::optional<net::IsolationInfo> isolation_info) {
  CHECK(network_context);

  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = network::mojom::kBrowserProcessId;
  factory_params->is_trusted = true;
  factory_params->is_corb_enabled = false;
  if (isolation_info) {
    factory_params->isolation_info = *isolation_info;
  }

  // Call WillCreateURLLoaderFactory so that Extensions (and other features) can
  // proxy the URLLoaderFactory pipe.
  RenderFrameHost* referring_render_frame_host =
      RenderFrameHost::FromID(referring_render_frame_host_id_);
  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      header_client;
  bool bypass_redirect_checks = false;
  GetContentClient()->browser()->WillCreateURLLoaderFactory(
      prefetch_service_->GetBrowserContext(), referring_render_frame_host,
      referring_render_frame_host->GetProcess()->GetID(),
      ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
      url::Origin::Create(referrer_.url),
      /*navigation_id=*/absl::nullopt,
      ukm::SourceIdObj::FromInt64(
          referring_render_frame_host->GetPageUkmSourceId()),
      &pending_receiver, &header_client, &bypass_redirect_checks,
      /*disable_secure_dns=*/nullptr, /*factory_override=*/nullptr,
      /*navigation_response_task_runner=*/nullptr);

  if (header_client.is_valid()) {
    factory_params->header_client = std::move(header_client);
  }

  network_context->CreateURLLoaderFactory(std::move(pending_receiver),
                                          std::move(factory_params));
}

}  // namespace content
