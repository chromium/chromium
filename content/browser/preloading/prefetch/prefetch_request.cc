// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_request.h"

#include <variant>

#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/preload_pipeline_info_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/prefetch_request_status_listener.h"

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
      ukm_source_id_(GetUkmSourceId(render_frame_host)) {}

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
    base::PassKey<PrefetchRequest>,
    const PrefetchType& prefetch_type,
    const PrefetchKey& key,
    const std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    std::optional<PrefetchPriority> priority,
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
    base::WeakPtr<PreloadingAttempt> attempt,
    base::WeakPtr<WebContents> referring_web_contents,
    bool is_javascript_enabled,
    const blink::mojom::Referrer& initial_referrer,
    const std::optional<url::Origin>& referring_origin,
    base::WeakPtr<BrowserContext> browser_context,
    std::optional<SpeculationRulesTags> speculation_rules_tags,
    const net::HttpRequestHeaders& additional_headers,
    base::TimeDelta ttl,
    PreloadingHoldbackStatus holdback_status_override,
    bool should_append_variations_header,
    bool should_disable_block_until_head_timeout,
    bool should_bypass_http_cache,
    std::variant<PrefetchRendererInitiatorInfo, PrefetchBrowserInitiatorInfo>
        initiator_info)
    : prefetch_type_(prefetch_type),
      key_(key),
      no_vary_search_hint_(std::move(no_vary_search_hint)),
      priority_(std::move(priority)),
      preload_pipeline_info_(base::WrapRefCounted(
          static_cast<PreloadPipelineInfoImpl*>(preload_pipeline_info.get()))),
      attempt_(std::move(attempt)),
      is_javascript_enabled_(is_javascript_enabled),
      initial_referrer_(initial_referrer),
      referring_origin_(referring_origin),
      referring_web_contents_(std::move(referring_web_contents)),
      browser_context_(std::move(browser_context)),
      speculation_rules_tags_(std::move(speculation_rules_tags)),
      additional_headers_(additional_headers),
      ttl_(std::move(ttl)),
      holdback_status_override_(std::move(holdback_status_override)),
      should_append_variations_header_(should_append_variations_header),
      should_disable_block_until_head_timeout_(
          should_disable_block_until_head_timeout),
      should_bypass_http_cache_(should_bypass_http_cache),
      initiator_info_(std::move(initiator_info)) {
  CHECK(preload_pipeline_info_);
  if (prefetch_type_.IsRendererInitiated()) {
    CHECK(GetRendererInitiatorInfo());
    CHECK(!GetBrowserInitiatorInfo());
    CHECK(additional_headers_.IsEmpty());
    CHECK_EQ(ttl_, PrefetchContainerDefaultTtlInPrefetchService());
    CHECK_EQ(holdback_status_override_, PreloadingHoldbackStatus::kUnspecified);
    CHECK(should_append_variations_header_);
    CHECK(!should_disable_block_until_head_timeout_);
  } else {
    CHECK(!GetRendererInitiatorInfo());
    CHECK(GetBrowserInitiatorInfo());
    CHECK(!speculation_rules_tags_);
  }
}

PrefetchRequest::~PrefetchRequest() = default;

// static
std::unique_ptr<const PrefetchRequest> PrefetchRequest::CreateRendererInitiated(
    RenderFrameHostImpl& referring_render_frame_host,
    const blink::DocumentToken& referring_document_token,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    std::optional<SpeculationRulesTags> speculation_rules_tags,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    std::optional<PrefetchPriority> priority,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager,
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
    base::WeakPtr<PreloadingAttempt> attempt) {
  return std::make_unique<PrefetchRequest>(
      base::PassKey<PrefetchRequest>(), prefetch_type,
      PrefetchKey(referring_document_token, url),
      std::move(no_vary_search_hint), std::move(priority),
      std::move(preload_pipeline_info), std::move(attempt),
      WebContentsImpl::FromRenderFrameHostImpl(&referring_render_frame_host)
          ->GetWeakPtr(),
      WebContentsImpl::FromRenderFrameHostImpl(&referring_render_frame_host)
          ->GetOrCreateWebPreferences()
          .javascript_enabled,
      referrer, referring_render_frame_host.GetLastCommittedOrigin(),
      referring_render_frame_host.GetBrowserContext()->GetWeakPtr(),
      std::move(speculation_rules_tags),
      /*Must be empty: additional_headers=*/net::HttpRequestHeaders(),
      PrefetchContainerDefaultTtlInPrefetchService(),
      /*holdback_status_override=*/PreloadingHoldbackStatus::kUnspecified,
      /*should_append_variations_header=*/true,
      /*should_disable_block_until_head_timeout=*/false,
      /*should_bypass_http_cache=*/false,
      PrefetchRendererInitiatorInfo(referring_render_frame_host,
                                    std::move(prefetch_document_manager)));
}

// static
std::unique_ptr<const PrefetchRequest> PrefetchRequest::CreateBrowserInitiated(
    WebContents& referring_web_contents,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const std::string& embedder_histogram_suffix,
    const blink::mojom::Referrer& referrer,
    const std::optional<url::Origin>& referring_origin,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    std::optional<PrefetchPriority> priority,
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
    base::WeakPtr<PreloadingAttempt> attempt,
    PreloadingHoldbackStatus holdback_status_override,
    std::optional<base::TimeDelta> ttl) {
  return std::make_unique<PrefetchRequest>(
      base::PassKey<PrefetchRequest>(), prefetch_type,
      PrefetchKey(std::optional<blink::DocumentToken>(std::nullopt), url),
      std::move(no_vary_search_hint), std::move(priority),
      std::move(preload_pipeline_info), std::move(attempt),
      referring_web_contents.GetWeakPtr(),
      referring_web_contents.GetOrCreateWebPreferences().javascript_enabled,
      referrer, referring_origin,
      referring_web_contents.GetBrowserContext()->GetWeakPtr(),
      /*speculation_rules_tags=*/std::nullopt,
      /*Must be empty: additional_headers=*/net::HttpRequestHeaders(),
      ttl.has_value() ? ttl.value()
                      : PrefetchContainerDefaultTtlInPrefetchService(),
      std::move(holdback_status_override),
      /*should_append_variations_header=*/true,
      /*should_disable_block_until_head_timeout=*/false,
      /*should_bypass_http_cache=*/false,
      PrefetchBrowserInitiatorInfo(embedder_histogram_suffix,
                                   /*request_status_listener=*/nullptr));
}

// static
std::unique_ptr<const PrefetchRequest>
PrefetchRequest::CreateBrowserInitiatedWithoutWebContents(
    BrowserContext* browser_context,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const std::string& embedder_histogram_suffix,
    const blink::mojom::Referrer& referrer,
    bool javascript_enabled,
    const std::optional<url::Origin>& referring_origin,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    std::optional<PrefetchPriority> priority,
    base::WeakPtr<PreloadingAttempt> attempt,
    const net::HttpRequestHeaders& additional_headers,
    std::unique_ptr<PrefetchRequestStatusListener> request_status_listener,
    base::TimeDelta ttl,
    bool should_append_variations_header,
    bool should_disable_block_until_head_timeout,
    bool should_bypass_http_cache) {
  return std::make_unique<PrefetchRequest>(
      base::PassKey<PrefetchRequest>(), prefetch_type,
      PrefetchKey(std::optional<blink::DocumentToken>(std::nullopt), url),
      std::move(no_vary_search_hint), std::move(priority),
      PreloadPipelineInfo::Create(
          /*planned_max_preloading_type=*/PreloadingType::kPrefetch),
      std::move(attempt), /*referring_web_contents=*/nullptr,
      javascript_enabled, referrer, referring_origin,
      browser_context->GetWeakPtr(),
      /*speculation_rules_tags=*/
      std::nullopt, additional_headers, ttl,
      /*holdback_status_override=*/PreloadingHoldbackStatus::kUnspecified,
      should_append_variations_header, should_disable_block_until_head_timeout,
      should_bypass_http_cache,
      PrefetchBrowserInitiatorInfo(embedder_histogram_suffix,
                                   std::move(request_status_listener)));
}

const PrefetchRendererInitiatorInfo* PrefetchRequest::GetRendererInitiatorInfo()
    const {
  return std::get_if<PrefetchRendererInitiatorInfo>(&initiator_info_);
}
const PrefetchBrowserInitiatorInfo* PrefetchRequest::GetBrowserInitiatorInfo()
    const {
  return std::get_if<PrefetchBrowserInitiatorInfo>(&initiator_info_);
}

}  // namespace content
