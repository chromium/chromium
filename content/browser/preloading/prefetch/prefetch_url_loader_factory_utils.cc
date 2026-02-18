// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_factory_utils.h"

#include "content/browser/loader/url_loader_factory_utils.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace content {

scoped_refptr<network::SharedURLLoaderFactory> CreatePrefetchURLLoaderFactory(
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
  factory_params->process_id = network::OriginatingProcessId::browser();
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
