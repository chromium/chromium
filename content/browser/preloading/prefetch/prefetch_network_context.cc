// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_network_context.h"

#include "base/memory/scoped_refptr.h"
#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"

namespace content {

PrefetchNetworkContext::PrefetchNetworkContext(
    bool use_isolated_network_context,
    mojo::Remote<network::mojom::NetworkContext> isolated_network_context,
    const PrefetchRequest& prefetch_request)
    : use_isolated_network_context_(use_isolated_network_context),
      network_context_(std::move(isolated_network_context)) {
  network::mojom::NetworkContext* network_context;
  if (use_isolated_network_context_) {
    network_context = network_context_.get();
  } else {
    CHECK(!network_context_);
    network_context = prefetch_request.browser_context()
                          ->GetDefaultStoragePartition()
                          ->GetNetworkContext();
  }
  CHECK(network_context);
  url_loader_factory_ =
      CreateNewURLLoaderFactory(network_context, prefetch_request);
  CHECK(url_loader_factory_);
}

PrefetchNetworkContext::~PrefetchNetworkContext() = default;

scoped_refptr<network::SharedURLLoaderFactory>
PrefetchNetworkContext::GetURLLoaderFactory() {
  return url_loader_factory_;
}

network::mojom::CookieManager* PrefetchNetworkContext::GetCookieManager() {
  CHECK(use_isolated_network_context_);
  CHECK(network_context_);
  if (!cookie_manager_) {
    network_context_->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());
  }

  return cookie_manager_.get();
}

void PrefetchNetworkContext::CloseIdleConnections() {
  if (network_context_) {
    network_context_->CloseIdleConnections(base::DoNothing());
  }
}

scoped_refptr<network::SharedURLLoaderFactory>
PrefetchNetworkContext::CreateNewURLLoaderFactory(
    network::mojom::NetworkContext* network_context,
    const PrefetchRequest& prefetch_request) {
  CHECK(network_context);

  RenderFrameHost* referring_render_frame_host;
  int referring_render_process_id;
  ukm::SourceIdObj ukm_source_id;
  if (auto* renderer_initiator_info =
          prefetch_request.GetRendererInitiatorInfo()) {
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

  bool bypass_redirect_checks = false;
  auto factory_params = network::mojom::URLLoaderFactoryParams::New();
  factory_params->process_id = network::OriginatingProcess::browser();
  factory_params->is_trusted = true;
  factory_params->is_orb_enabled = false;
  return url_loader_factory::Create(
      ContentBrowserClient::URLLoaderFactoryType::kPrefetch,
      url_loader_factory::TerminalParams::ForNetworkContext(
          network_context, std::move(factory_params),
          url_loader_factory::HeaderClientOption::kAllow),
      url_loader_factory::ContentClientParams(
          prefetch_request.browser_context(), referring_render_frame_host,
          referring_render_process_id,
          prefetch_request.referring_origin().value_or(url::Origin()),
          net::IsolationInfo(), ukm_source_id, &bypass_redirect_checks));
}

}  // namespace content
