// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"

#include "base/check_is_test.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

namespace {
static network::SharedURLLoaderFactory* g_url_loader_factory_for_testing =
    nullptr;
}  // namespace

network::mojom::URLLoaderFactoryParamsPtr CreatePrefetchURLLoaderFactoryParams(
    const base::UnguessableToken& network_restrictions_id) {
  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = network::OriginatingProcessId::browser();
  factory_params->is_trusted = true;
  factory_params->is_orb_enabled = false;
  factory_params->network_restrictions_id = network_restrictions_id;
  return factory_params;
}

void SetTerminalPrefetchURLLoaderFactoryForTesting(  // IN-TEST
    network::SharedURLLoaderFactory* url_loader_factory) {
  g_url_loader_factory_for_testing = url_loader_factory;
}

// `browser_context`, `referring_origin`, and `renderer_initiator_info` should
// match with the corresponding fields of `PrefetchRequest`.
template <typename FactoryType>
FactoryType CreatePrefetchURLLoaderFactory(
    network::mojom::NetworkContext* network_context,
    BrowserContext* browser_context,
    const std::optional<url::Origin>& referring_origin,
    const PrefetchRendererInitiatorInfo* renderer_initiator_info,
    scoped_refptr<network::SharedURLLoaderFactory>
        pre_prefetch_url_loader_factory = nullptr) {
  CHECK(network_context);

  RenderFrameHost* referring_render_frame_host;
  int referring_render_process_id;
  ukm::SourceIdObj ukm_source_id;
  if (renderer_initiator_info) {
    referring_render_frame_host = renderer_initiator_info->GetRenderFrameHost();
    CHECK(referring_render_frame_host);
    referring_render_process_id =
        referring_render_frame_host->GetProcess()->GetDeprecatedID();
    ukm_source_id =
        ukm::SourceIdObj::FromInt64(renderer_initiator_info->ukm_source_id());
  } else {
    referring_render_frame_host = nullptr;
    referring_render_process_id = content::ChildProcessHost::kInvalidUniqueID;
    ukm_source_id = ukm::kInvalidSourceIdObj;
  }

  base::UnguessableToken network_restrictions_id =
      network::GetNoOpNetworkRestrictionsId();
  if (referring_render_frame_host) {
    auto* rfh_impl =
        static_cast<RenderFrameHostImpl*>(referring_render_frame_host);
    network_restrictions_id = rfh_impl->GetNetworkRestrictionsID();
  }

  bool bypass_redirect_checks = false;

  url_loader_factory::TerminalParams terminal_params = [&]() {
    // If this is for PrePrefetch-promoted request, serve from the PrePrefetched
    // result.
    if (pre_prefetch_url_loader_factory) {
      return url_loader_factory::TerminalParams::ForNonNetwork(
          std::move(pre_prefetch_url_loader_factory),
          network::mojom::kBrowserProcessId);
    }

    // Intercept the request for testing, if any (but not for
    // PrePrefetch-promoted cases (which is done in
    // `CreatePrePrefetchURLLoaderFactoryOnUI()` below), see the method comment
    // in the header).
    if (g_url_loader_factory_for_testing) {
      return url_loader_factory::TerminalParams::ForNonNetwork(
          base::WrapRefCounted(g_url_loader_factory_for_testing),
          network::mojom::kBrowserProcessId);
    }

    // Otherwise, send the request to the network.
    return url_loader_factory::TerminalParams::ForNetworkContext(
        network_context,
        CreatePrefetchURLLoaderFactoryParams(network_restrictions_id),
        url_loader_factory::HeaderClientOption::kAllow);
  }();

  return url_loader_factory::CreateInternal<FactoryType>(
      ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
      std::move(terminal_params),
      url_loader_factory::ContentClientParams(
          browser_context, referring_render_frame_host,
          referring_render_process_id, referring_origin.value_or(url::Origin()),
          net::IsolationInfo(), ukm_source_id, &bypass_redirect_checks),
      /*devtools_params=*/std::nullopt);
}

scoped_refptr<network::SharedURLLoaderFactory> CreatePrefetchURLLoaderFactory(
    network::mojom::NetworkContext* network_context,
    const PrefetchRequest& prefetch_request,
    scoped_refptr<network::SharedURLLoaderFactory>
        pre_prefetch_url_loader_factory) {
  return CreatePrefetchURLLoaderFactory<
      scoped_refptr<network::SharedURLLoaderFactory>>(
      network_context, prefetch_request.browser_context(),
      prefetch_request.referring_origin(),
      prefetch_request.GetRendererInitiatorInfo(),
      std::move(pre_prefetch_url_loader_factory));
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreatePrePrefetchURLLoaderFactoryOnUI(BrowserContext* browser_context) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  CHECK(browser_context);

  // This is the same default network context that should be used in normal
  // prefetch's `URLLoaderFactory` on the UI thread, created via
  // `PrefetchContainer::GetOrCreateDefaultNetworkContextURLLoaderFactory()`.
  network::mojom::NetworkContext* network_context =
      browser_context->GetDefaultStoragePartition()->GetNetworkContext();

  if (features::kPrefetchOffTheMainThreadCheckWillCreateURLLoaderFactory
          .Get()) {
    // Creates the same URLLoaderFactory as the PrefetchContainer's default
    // network context's URLLoaderFactory. This is only for Android WebView
    // prefetches, assuming:
    // - Always browser-initiated.
    // - Referring origin is always `std::nullopt` (see
    //   `BrowserContext::StartBrowserPrefetchRequest()`).
    return CreatePrefetchURLLoaderFactory<
        mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
        network_context, browser_context,
        /*referring_origin=*/std::nullopt,
        /*renderer_initiator_info=*/nullptr);
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_factory;
  if (g_url_loader_factory_for_testing) {
    CHECK_IS_TEST();
    g_url_loader_factory_for_testing->Clone(
        pending_factory.InitWithNewPipeAndPassReceiver());
  } else {
    // Unlike `CreatePrefetchURLLoaderFactory()`, this does use
    // `url_loader_factory::HeaderClientOption::kDisallow` and null
    // `url_loader_factory::ContentClientParams`. The interceptors that would be
    // added by `ContentClientParams` will be added/executed when the
    // PrePrefetch is consumed by a `PrefetchContainer`.
    pending_factory = url_loader_factory::CreatePendingRemote(
        ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
        url_loader_factory::TerminalParams::ForNetworkContext(
            network_context,
            CreatePrefetchURLLoaderFactoryParams(
                // Pre-prefetches are browser-initiated without a referring
                // frame/context, so no creator restrictions apply.
                network::GetNoOpNetworkRestrictionsId()),
            url_loader_factory::HeaderClientOption::kDisallow),
        /*content_client_params=*/std::nullopt);
  }
  return pending_factory;
}

}  // namespace content
