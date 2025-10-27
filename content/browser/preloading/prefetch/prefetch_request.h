// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_REQUEST_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_REQUEST_H_

#include <optional>
#include <variant>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_key.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "content/public/browser/preloading.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_headers.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "url/origin.h"

namespace content {

class BrowserContext;
class PrefetchDocumentManager;
class PreloadPipelineInfo;
class PreloadPipelineInfoImpl;
class PreloadingAttempt;
class RenderFrameHostImpl;
class WebContents;
enum class PrefetchPriority;

// `PrefetchRendererInitiatorInfo` or `PrefetchBrowserInitiatorInfo` is created
// and attached to `PrefetchRequest` for a renderer-initiated or
// browser-initiated prefetch, respectively.
//
// They represents the initiator information and other request parameters
// specific to renderer-initiated or browser-initiated prefetches.

// For renderer-initiated prefetches: the initiator Document and its
// `RenderFrameHost`, `PrefetchDocumentManager`, etc.
class CONTENT_EXPORT PrefetchRendererInitiatorInfo final {
 public:
  PrefetchRendererInitiatorInfo(
      RenderFrameHostImpl& render_frame_host,
      base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager);
  ~PrefetchRendererInitiatorInfo();

  // Move-only.
  PrefetchRendererInitiatorInfo(const PrefetchRendererInitiatorInfo&) = delete;
  PrefetchRendererInitiatorInfo& operator=(
      const PrefetchRendererInitiatorInfo&) = delete;
  PrefetchRendererInitiatorInfo(PrefetchRendererInitiatorInfo&&);

  GlobalRenderFrameHostId GetRenderFrameHostId() const {
    return render_frame_host_id_;
  }
  RenderFrameHostImpl* GetRenderFrameHost() const;
  PrefetchDocumentManager* prefetch_document_manager() const {
    return prefetch_document_manager_.get();
  }
  const std::optional<base::UnguessableToken>& devtools_navigation_token()
      const {
    return devtools_navigation_token_;
  }
  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }

 private:
  // The RenderFrameHostId/PrefetchDocumentManager of the Document that
  // triggered the prefetch.
  GlobalRenderFrameHostId render_frame_host_id_;

  base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager_;

  // A DevTools token used to identify initiator document.
  std::optional<base::UnguessableToken> devtools_navigation_token_;

  ukm::SourceId ukm_source_id_;
};

// For browser-initiated prefetches.
class CONTENT_EXPORT PrefetchBrowserInitiatorInfo final {
 public:
  PrefetchBrowserInitiatorInfo(
      const std::string& embedder_histogram_suffix,
      std::unique_ptr<PrefetchRequestStatusListener> request_status_listener);
  ~PrefetchBrowserInitiatorInfo();

  // Move-only.
  PrefetchBrowserInitiatorInfo(const PrefetchBrowserInitiatorInfo&) = delete;
  PrefetchBrowserInitiatorInfo& operator=(const PrefetchBrowserInitiatorInfo&) =
      delete;
  PrefetchBrowserInitiatorInfo(PrefetchBrowserInitiatorInfo&&);

  const std::string& embedder_histogram_suffix() const {
    return embedder_histogram_suffix_;
  }
  PrefetchRequestStatusListener* request_status_listener() const {
    return request_status_listener_.get();
  }

 private:
  // The suffix string of embedder triggers used for generating histogram
  // recorded per trigger.
  std::string embedder_histogram_suffix_;

  // Listener of prefetch request. Currently used for WebView initiated
  // prefetch.
  std::unique_ptr<PrefetchRequestStatusListener> request_status_listener_;
};

// `PrefetchRequest` represents request parameters to `PrefetchService` to
// prefetch a URL.
//
// TODO(https://crbug.com/437631382): Incrementally migrate the
// `PrefetchContainer` constructor arguments into `PrefetchRequest`, so that
// eventually all parameters are passed via `PrefetchRequest` from the current
// callers of the `PrefetchContainer` constructors. After that, we can make the
// callers to construct `PrefetchRequest` instead of `PrefetchContainer`.
class CONTENT_EXPORT PrefetchRequest final {
 public:
  // For renderer-initiated prefetch.
  static std::unique_ptr<const PrefetchRequest> CreateRendererInitiated(
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
      base::WeakPtr<PreloadingAttempt> attempt = nullptr);

  // For browser-initiated prefetch.
  // We can pass the referring origin of prefetches via `referring_origin` if
  // necessary.
  static std::unique_ptr<const PrefetchRequest> CreateBrowserInitiated(
      WebContents& referring_web_contents,
      const GURL& url,
      const PrefetchType& prefetch_type,
      const std::string& embedder_histogram_suffix,
      const blink::mojom::Referrer& referrer,
      const std::optional<url::Origin>& referring_origin,
      std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
      std::optional<PrefetchPriority> priority,
      scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
      base::WeakPtr<PreloadingAttempt> attempt = nullptr,
      PreloadingHoldbackStatus holdback_status_override =
          PreloadingHoldbackStatus::kUnspecified,
      std::optional<base::TimeDelta> ttl = std::nullopt);

  // For browser-initiated prefetch that doesn't depend on web
  // contents. We can pass the referring origin of prefetches via
  // `referring_origin` if necessary.
  static std::unique_ptr<const PrefetchRequest>
  CreateBrowserInitiatedWithoutWebContents(
      BrowserContext* browser_context,
      const GURL& url,
      const PrefetchType& prefetch_type,
      const std::string& embedder_histogram_suffix,
      const blink::mojom::Referrer& referrer,
      bool javascript_enabled,
      const std::optional<url::Origin>& referring_origin,
      std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
      std::optional<PrefetchPriority> priority,
      base::WeakPtr<PreloadingAttempt> attempt = nullptr,
      const net::HttpRequestHeaders& additional_headers = {},
      std::unique_ptr<PrefetchRequestStatusListener> request_status_listener =
          nullptr,
      base::TimeDelta ttl = PrefetchContainerDefaultTtlInPrefetchService(),
      bool should_append_variations_header = true,
      bool should_disable_block_until_head_timeout = false,
      bool should_bypass_http_cache = false);

  // Use `Create*()` above instead.
  PrefetchRequest(
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
          info);

  ~PrefetchRequest();

  const PrefetchType& prefetch_type() const { return prefetch_type_; }
  const PrefetchKey& key() const { return key_; }
  const std::optional<net::HttpNoVarySearchData>& no_vary_search_hint() const {
    return no_vary_search_hint_;
  }
  const std::optional<PrefetchPriority>& priority() const { return priority_; }
  PreloadPipelineInfoImpl& preload_pipeline_info() const {
    return *preload_pipeline_info_;
  }
  PreloadingAttempt* attempt() const { return attempt_.get(); }
  bool is_javascript_enabled() const { return is_javascript_enabled_; }
  const blink::mojom::Referrer& initial_referrer() const {
    return initial_referrer_;
  }
  const std::optional<url::Origin>& referring_origin() const {
    return referring_origin_;
  }
  const base::WeakPtr<WebContents>& referring_web_contents() const {
    return referring_web_contents_;
  }
  BrowserContext* browser_context() const { return browser_context_.get(); }

  const std::optional<SpeculationRulesTags>& speculation_rules_tags() const {
    return speculation_rules_tags_;
  }

  const net::HttpRequestHeaders& additional_headers() const {
    return additional_headers_;
  }
  const base::TimeDelta& ttl() const { return ttl_; }
  PreloadingHoldbackStatus holdback_status_override() const {
    return holdback_status_override_;
  }
  bool should_append_variations_header() const {
    return should_append_variations_header_;
  }
  bool should_disable_block_until_head_timeout() const {
    return should_disable_block_until_head_timeout_;
  }
  // TODO(crbug.com/455296998): Remove this code for M145.
  bool should_bypass_http_cache() const { return should_bypass_http_cache_; }

  // Returns non-null if renderer-initiated/browser-initiated, respectively.
  // Exactly one of them returns non-null.
  const PrefetchRendererInitiatorInfo* GetRendererInitiatorInfo() const;
  const PrefetchBrowserInitiatorInfo* GetBrowserInitiatorInfo() const;

 private:
  // The type of this prefetch. This controls some specific details about how
  // the prefetch is handled, including whether an isolated network context or
  // the default network context is used to perform the prefetch, whether or
  // not the preftch proxy is used, and whether or not subresources are
  // prefetched.
  const PrefetchType prefetch_type_;

  // The key used to match this PrefetchContainer, including the URL that was
  // requested to prefetch.
  const PrefetchKey key_;

  // The No-Vary-Search hint of the prefetch, which is specified by the
  // speculation rules and can be different from actual
  // `PrefetchContainer::no_vary_search_data_`.
  const std::optional<net::HttpNoVarySearchData> no_vary_search_hint_;

  // An optimization hint indicating how quickly this prefetch should be
  // available.
  const std::optional<PrefetchPriority> priority_;

  // The primary `PreloadPipelineInfo`, i.e. that of the first initiator.
  // Subsequent requests' `PreloadPipelineInfo`s are tracked by
  // `PrefetchContainer::inherited_preload_pipeline_infos_`.
  // The pointer is immutable (i.e. pointing to the same
  // `PreloadPipelineInfoImpl` throughout the lifetime) while the pointed
  // PreloadPipelineInfoImpl is mutable as we'll update/notify the
  // PreloadPipelineInfoImpl.
  const scoped_refptr<PreloadPipelineInfoImpl> preload_pipeline_info_;

  // `PreloadingAttempt` is used to track the lifecycle of the preloading event,
  // and reports various statuses to UKM dashboard. It is initialised along with
  // `this`, and destroyed when `WCO::DidFinishNavigation` is fired.
  // `attempt_`'s eligibility is set in `OnEligibilityCheckComplete`, and its
  // holdback status, triggering outcome and failure reason are set in
  // `SetPrefetchStatus`.
  const base::WeakPtr<PreloadingAttempt> attempt_;

  // Whether JavaScript is on in this contents (or was, when this prefetch
  // started). This affects Client Hints behavior. Per-origin settings are
  // handled later, according to
  // |ClientHintsControllerDelegate::IsJavaScriptAllowed|.
  const bool is_javascript_enabled_;

  // The referrer to use for the initial request.
  // Only for initialization of `PrefetchContainer::referrer_`.
  // For other cases, use `PrefetchContainer::referrer_` instead.
  const blink::mojom::Referrer initial_referrer_;

  // The origin and URL that initiates the prefetch request.
  // For renderer-initiated prefetch, this is calculated by referring
  // RenderFrameHost's LastCommittedOrigin. For browser-initiated prefetch, this
  // is sometimes explicitly passed via ctor.
  const std::optional<url::Origin> referring_origin_;

  // The initiator WebContents used for the source of some headers, e.g.,
  // User-Agent. This is unavailable in browser initiated prefetch that is not
  // associated with WebContents. This is for an initial guess and shouldn't be
  // used without a plan for the header validation (crbug.com/444065296).
  base::WeakPtr<WebContents> referring_web_contents_;

  // The |BrowserContext| in which this is being run.
  const base::WeakPtr<BrowserContext> browser_context_;

  // -------- Parameters that can have non-default values only for
  // -------- renderer-initiated prefetches:

  // The tags of the speculation rules that triggered this prefetch, and this
  // field is non-null if and only if this is created by SpeculationRules
  // prefech. These are assumed to have been validated by the time this is
  // constructed.
  const std::optional<SpeculationRulesTags> speculation_rules_tags_;

  // -------- Parameters that can have non-default values only for
  // -------- browser-initiated prefetches:

  // Additional headers for WebView initiated prefetch.
  // This must be empty for non-WebView initiated prefetches.
  // TODO(crbug.com/369859822): Revisit the semantics if needed.
  const net::HttpRequestHeaders additional_headers_;

  // Time-to-live (TTL) for this prefetched data. Currently, this is configured
  // for browser-initiated prefetch that doesn't depend on web content.
  // Default value is `PrefetchContainerDefaultTtlInPrefetchService()`.
  const base::TimeDelta ttl_;

  // If not `PreloadingHoldbackStatus::kUnspecified`, this value is used to
  // override holdback status derived by the normal process. It is set to
  // `attempt_` on PrefetchService::CheckAndSetPrefetchHoldbackStatus(). Default
  // value is `PreloadingHoldbackStatus::kUnspecified`.
  const PreloadingHoldbackStatus holdback_status_override_;

  // Whether to add the X-Client-Data header with experiment IDs from field
  // trials. This will not be applied to redirects. Currently, this is
  // configured for browser-initiated prefetch that doesn't depend on web
  // content. Default value is `true`.
  const bool should_append_variations_header_;

  // Whether the caller of prefetches requests to disable
  // `BlockUntilHeadTimeout`, which is currently calculated by
  // `PrefetchBlockUntilHeadTimeout()` as a `prefetch_params`.
  // Default value is `false`.
  const bool should_disable_block_until_head_timeout_;

  // Whether the prefetch request should use the HTTP cache.
  // Default value is `false`.
  // TODO(crbug.com/455296998): Remove this code for M145.
  const bool should_bypass_http_cache_;

  const std::variant<PrefetchRendererInitiatorInfo,
                     PrefetchBrowserInitiatorInfo>
      initiator_info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_REQUEST_H_
