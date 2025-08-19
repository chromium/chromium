// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_request.h"

#include <variant>

#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/preload_pipeline_info_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "url/gurl.h"

namespace content {
namespace {

ukm::SourceId GetUkmSourceId(RenderFrameHostImpl& rfhi) {
  // Prerendering page should not trigger prefetches.
  CHECK(
      !rfhi.IsInLifecycleState(RenderFrameHost::LifecycleState::kPrerendering));
  return rfhi.GetPageUkmSourceId();
}

}  // namespace

PrefetchRendererInitiatorInfo::PrefetchRendererInitiatorInfo(
    RenderFrameHostImpl& render_frame_host,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager)
    : render_frame_host_id_(render_frame_host.GetGlobalId()),
      prefetch_document_manager_(std::move(prefetch_document_manager)),
      devtools_navigation_token_(
          render_frame_host.GetDevToolsNavigationToken()),
      ukm_source_id_(GetUkmSourceId(render_frame_host)),
      url_hash_(
          base::FastHash(render_frame_host.GetLastCommittedURL().spec())) {}

PrefetchRendererInitiatorInfo::PrefetchRendererInitiatorInfo(
    PrefetchRendererInitiatorInfo&&) = default;
PrefetchRendererInitiatorInfo::~PrefetchRendererInitiatorInfo() = default;

RenderFrameHostImpl* PrefetchRendererInitiatorInfo::GetRenderFrameHost() const {
  return RenderFrameHostImpl::FromID(render_frame_host_id_);
}

PrefetchBrowserInitiatorInfo::~PrefetchBrowserInitiatorInfo() = default;
PrefetchBrowserInitiatorInfo::PrefetchBrowserInitiatorInfo(
    PrefetchBrowserInitiatorInfo&&) = default;

PrefetchBrowserInitiatorInfo::PrefetchBrowserInitiatorInfo(
    const std::string& embedder_histogram_suffix,
    std::unique_ptr<PrefetchRequestStatusListener> request_status_listener)
    : embedder_histogram_suffix_(embedder_histogram_suffix),
      request_status_listener_(std::move(request_status_listener)) {
  CHECK(!embedder_histogram_suffix_.empty());
}

PrefetchRequest::PrefetchRequest(
    const PrefetchType& prefetch_type,
    const PrefetchKey& key,
    const std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
    base::WeakPtr<PreloadingAttempt> attempt,
    bool is_javascript_enabled,
    const std::optional<url::Origin>& referring_origin,
    base::WeakPtr<BrowserContext> browser_context,
    std::optional<SpeculationRulesTags> speculation_rules_tags,
    const net::HttpRequestHeaders& additional_headers,
    std::optional<PreloadingHoldbackStatus> holdback_status_override,
    std::variant<PrefetchRendererInitiatorInfo, PrefetchBrowserInitiatorInfo>
        initiator_info)
    : prefetch_type_(prefetch_type),
      key_(key),
      no_vary_search_hint_(std::move(no_vary_search_hint)),
      preload_pipeline_info_(base::WrapRefCounted(
          static_cast<PreloadPipelineInfoImpl*>(preload_pipeline_info.get()))),
      attempt_(std::move(attempt)),
      is_javascript_enabled_(is_javascript_enabled),
      referring_origin_(referring_origin),
      browser_context_(std::move(browser_context)),
      speculation_rules_tags_(std::move(speculation_rules_tags)),
      additional_headers_(additional_headers),
      holdback_status_override_(std::move(holdback_status_override)),
      initiator_info_(std::move(initiator_info)) {
  CHECK(preload_pipeline_info_);
  if (prefetch_type_.IsRendererInitiated()) {
    CHECK(GetRendererInitiatorInfo());
    CHECK(!GetBrowserInitiatorInfo());
    CHECK(additional_headers_.IsEmpty());
    CHECK(!holdback_status_override_);
  } else {
    CHECK(!GetRendererInitiatorInfo());
    CHECK(GetBrowserInitiatorInfo());
    CHECK(!speculation_rules_tags_);
  }
}

PrefetchRequest::~PrefetchRequest() = default;

const PrefetchRendererInitiatorInfo* PrefetchRequest::GetRendererInitiatorInfo()
    const {
  return std::get_if<PrefetchRendererInitiatorInfo>(&initiator_info_);
}
const PrefetchBrowserInitiatorInfo* PrefetchRequest::GetBrowserInitiatorInfo()
    const {
  return std::get_if<PrefetchBrowserInitiatorInfo>(&initiator_info_);
}

}  // namespace content
