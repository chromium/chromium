// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_request.h"

#include <variant>

#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/global_routing_id.h"
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
    const std::string& embedder_histogram_suffix)
    : embedder_histogram_suffix_(embedder_histogram_suffix) {
  CHECK(!embedder_histogram_suffix_.empty());
}

PrefetchRequest::PrefetchRequest(
    const PrefetchType& prefetch_type,
    const PrefetchKey& key,
    const std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    const std::optional<url::Origin>& referring_origin,
    std::optional<SpeculationRulesTags> speculation_rules_tags,
    std::variant<PrefetchRendererInitiatorInfo, PrefetchBrowserInitiatorInfo>
        initiator_info)
    : prefetch_type_(prefetch_type),
      key_(key),
      no_vary_search_hint_(std::move(no_vary_search_hint)),
      referring_origin_(referring_origin),
      speculation_rules_tags_(std::move(speculation_rules_tags)),
      initiator_info_(std::move(initiator_info)) {
  if (prefetch_type_.IsRendererInitiated()) {
    CHECK(GetRendererInitiatorInfo());
    CHECK(!GetBrowserInitiatorInfo());
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
