// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include <memory>
#include <variant>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/proxy_lookup_client_impl.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/client_hints.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_util.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"
#include "url/gurl.h"

namespace content {
namespace {

void RecordCookieCopyTimes(
    const base::TimeTicks& cookie_copy_start_time,
    const base::TimeTicks& cookie_read_end_and_write_start_time,
    const base::TimeTicks& cookie_copy_end_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieReadTime",
      cookie_read_end_and_write_start_time - cookie_copy_start_time,
      base::TimeDelta(), base::Seconds(5), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieWriteTime",
      cookie_copy_end_time - cookie_read_end_and_write_start_time,
      base::TimeDelta(), base::Seconds(5), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyTime",
      cookie_copy_end_time - cookie_copy_start_time, base::TimeDelta(),
      base::Seconds(5), 50);
}

PrefetchStatus PrefetchStatusFromIneligibleReason(
    PreloadingEligibility eligibility) {
  switch (eligibility) {
    case PreloadingEligibility::kBatterySaverEnabled:
      return PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled;
    case PreloadingEligibility::kDataSaverEnabled:
      return PrefetchStatus::kPrefetchIneligibleDataSaverEnabled;
    case PreloadingEligibility::kExistingProxy:
      return PrefetchStatus::kPrefetchIneligibleExistingProxy;
    case PreloadingEligibility::kHostIsNonUnique:
      return PrefetchStatus::kPrefetchIneligibleHostIsNonUnique;
    case PreloadingEligibility::kNonDefaultStoragePartition:
      return PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition;
    case PreloadingEligibility::kPrefetchProxyNotAvailable:
      return PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable;
    case PreloadingEligibility::kPreloadingDisabled:
      return PrefetchStatus::kPrefetchIneligiblePreloadingDisabled;
    case PreloadingEligibility::kRetryAfter:
      return PrefetchStatus::kPrefetchIneligibleRetryAfter;
    case PreloadingEligibility::kSameSiteCrossOriginPrefetchRequiredProxy:
      return PrefetchStatus::
          kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy;
    case PreloadingEligibility::kSchemeIsNotHttps:
      return PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps;
    case PreloadingEligibility::kUserHasCookies:
      return PrefetchStatus::kPrefetchIneligibleUserHasCookies;
    case PreloadingEligibility::kUserHasServiceWorker:
      return PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker;
    case PreloadingEligibility::kUserHasServiceWorkerNoFetchHandler:
      return PrefetchStatus::
          kPrefetchIneligibleUserHasServiceWorkerNoFetchHandler;
    case PreloadingEligibility::kRedirectFromServiceWorker:
      return PrefetchStatus::kPrefetchIneligibleRedirectFromServiceWorker;
    case PreloadingEligibility::kRedirectToServiceWorker:
      return PrefetchStatus::kPrefetchIneligibleRedirectToServiceWorker;

    case PreloadingEligibility::kEligible:
    default:
      // Other ineligible cases are not used in `PrefetchService`.
      NOTREACHED();
  }
}

std::optional<PreloadingTriggeringOutcome> TriggeringOutcomeFromStatus(
    PrefetchStatus prefetch_status) {
  switch (prefetch_status) {
    case PrefetchStatus::kPrefetchNotFinishedInTime:
      return PreloadingTriggeringOutcome::kRunning;
    case PrefetchStatus::kPrefetchSuccessful:
      return PreloadingTriggeringOutcome::kReady;
    case PrefetchStatus::kPrefetchResponseUsed:
      return PreloadingTriggeringOutcome::kSuccess;
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchFailedNetError:
    case PrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
    case PrefetchStatus::kPrefetchFailedInvalidRedirect:
    case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
    case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
    case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorkerNoFetchHandler:
    case PrefetchStatus::kPrefetchIneligibleRedirectFromServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleRedirectToServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
    case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
    case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
    case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
    case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
    case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
    case PrefetchStatus::kPrefetchIneligibleExistingProxy:
    case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::
        kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
    case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
    case PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved:
      return PreloadingTriggeringOutcome::kFailure;
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchNotStarted:
      return std::nullopt;
  }
  return std::nullopt;
}

// Returns true if SetPrefetchStatus(|status|) can be called after a prefetch
// has already been marked as failed. We ignore such status updates
// as they may end up overwriting the initial failure reason.
bool StatusUpdateIsPossibleAfterFailure(PrefetchStatus status) {
  switch (status) {
    case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
    case PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved: {
      CHECK(TriggeringOutcomeFromStatus(status) ==
            PreloadingTriggeringOutcome::kFailure);
      return true;
    }
    case PrefetchStatus::kPrefetchNotFinishedInTime:
    case PrefetchStatus::kPrefetchSuccessful:
    case PrefetchStatus::kPrefetchResponseUsed:
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchStatus::kPrefetchFailedNetError:
    case PrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
    case PrefetchStatus::kPrefetchFailedInvalidRedirect:
    case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorkerNoFetchHandler:
    case PrefetchStatus::kPrefetchIneligibleRedirectFromServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleRedirectToServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
    case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
    case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
    case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
    case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
    case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
    case PrefetchStatus::kPrefetchIneligibleExistingProxy:
    case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::
        kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchNotStarted:
    case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
      return false;
  }
}

ukm::SourceId GetUkmSourceId(RenderFrameHostImpl& rfhi) {
  // Prerendering page should not trigger prefetches.
  CHECK(
      !rfhi.IsInLifecycleState(RenderFrameHost::LifecycleState::kPrerendering));
  return rfhi.GetPageUkmSourceId();
}

void RecordPrefetchProxyPrefetchMainframeNetError(int net_error) {
  base::UmaHistogramSparse("PrefetchProxy.Prefetch.Mainframe.NetError",
                           std::abs(net_error));
}

void RecordPrefetchProxyPrefetchMainframeBodyLength(int64_t body_length) {
  UMA_HISTOGRAM_COUNTS_10M("PrefetchProxy.Prefetch.Mainframe.BodyLength",
                           body_length);
}

bool CalculateIsLikelyAheadOfPrerender(
    const PreloadPipelineInfoImpl& preload_pipeline_info) {
  if (!features::UsePrefetchPrerenderIntegration()) {
    return false;
  }

  switch (preload_pipeline_info.planned_max_preloading_type()) {
    case PreloadingType::kPrefetch:
      return false;
    case PreloadingType::kPrerender:
      return true;
    case PreloadingType::kUnspecified:
    case PreloadingType::kPreconnect:
    case PreloadingType::kNoStatePrefetch:
    case PreloadingType::kLinkPreview:
      NOTREACHED();
  }
}

PrefetchContainer::PrefetchResponseCompletedCallbackForTesting&
GetPrefetchResponseCompletedCallbackForTesting() {
  static base::NoDestructor<
      PrefetchContainer::PrefetchResponseCompletedCallbackForTesting>
      prefetch_response_completed_callback_for_testing;
  return *prefetch_response_completed_callback_for_testing;
}

}  // namespace

// Holds the state for the request for a single URL in the context of the
// broader prefetch. A prefetch can request multiple URLs due to redirects.
// const/mutable member convention:
// ------------------------ ----------- -------
// can be modified during:  prefetching serving
// ------------------------ ----------- -------
// const                    No          No
// non-const/non-mutable    Yes         No
// mutable                  Yes         Yes
// ------------------------ ----------- -------
// because const references are used via `GetCurrentSinglePrefetchToServe()`
// during serving.
class PrefetchContainer::SinglePrefetch {
 public:
  explicit SinglePrefetch(const GURL& url,
                          bool is_isolated_network_context_required,
                          bool is_reusable);
  ~SinglePrefetch();

  SinglePrefetch(const SinglePrefetch&) = delete;
  SinglePrefetch& operator=(const SinglePrefetch&) = delete;

  // The URL that will potentially be prefetched. This can be the original
  // prefetch URL, or a URL from a redirect resulting from requesting the
  // original prefetch URL.
  const GURL url_;

  const bool is_isolated_network_context_required_;

  // Whether this |url_| is eligible to be prefetched
  std::optional<PreloadingEligibility> eligibility_;

  // This tracks whether the cookies associated with |url_| have changed at
  // some point after the initial eligibility check.
  std::unique_ptr<PrefetchCookieListener> cookie_listener_;

  scoped_refptr<PrefetchResponseReader> response_reader_;

  // The different possible states of the cookie copy process.
  enum class CookieCopyStatus {
    kNotStarted,
    kInProgress,
    kCompleted,
  };

  // The current state of the cookie copy process for this prefetch.
  mutable CookieCopyStatus cookie_copy_status_ = CookieCopyStatus::kNotStarted;

  // The timestamps of when the overall cookie copy process starts, and midway
  // when the cookies are read from the isolated network context and are about
  // to be written to the default network context.
  mutable std::optional<base::TimeTicks> cookie_copy_start_time_;
  mutable std::optional<base::TimeTicks> cookie_read_end_and_write_start_time_;

  // A callback that runs once |cookie_copy_status_| is set to |kCompleted|.
  mutable base::OnceClosure on_cookie_copy_complete_callback_;
};

PrefetchContainer::PrefetchContainer(
    RenderFrameHostImpl& referring_render_frame_host,
    const blink::DocumentToken& referring_document_token,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    std::optional<SpeculationRulesTags> speculation_rules_tags,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager,
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
    base::WeakPtr<PreloadingAttempt> attempt)
    : PrefetchContainer(
          referring_render_frame_host.GetGlobalId(),
          referring_render_frame_host.GetLastCommittedOrigin(),
          base::FastHash(
              referring_render_frame_host.GetLastCommittedURL().spec()),
          PrefetchContainer::Key(referring_document_token, url),
          prefetch_type,
          /*embedder_histogram_suffix=*/std::nullopt,
          referrer,
          std::move(speculation_rules_tags),
          std::move(no_vary_search_hint),
          prefetch_document_manager,
          referring_render_frame_host.GetBrowserContext()->GetWeakPtr(),
          GetUkmSourceId(referring_render_frame_host),
          std::move(preload_pipeline_info),
          std::move(attempt),
          /*holdback_status_override=*/std::nullopt,
          referring_render_frame_host.GetDevToolsNavigationToken(),
          /*Must be empty: additional_headers=*/{},
          /*request_status_listener=*/nullptr,
          WebContentsImpl::FromRenderFrameHostImpl(&referring_render_frame_host)
              ->GetOrCreateWebPreferences()
              .javascript_enabled,
          PrefetchContainerDefaultTtlInPrefetchService(),
          /*should_append_variations_header=*/true) {
  CHECK(prefetch_type_.IsRendererInitiated());
}

PrefetchContainer::PrefetchContainer(
    WebContents& referring_web_contents,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const std::string& embedder_histogram_suffix,
    const blink::mojom::Referrer& referrer,
    const std::optional<url::Origin>& referring_origin,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
    base::WeakPtr<PreloadingAttempt> attempt,
    std::optional<PreloadingHoldbackStatus> holdback_status_override)
    : PrefetchContainer(
          GlobalRenderFrameHostId(),
          referring_origin,
          /*referring_url_hash=*/std::nullopt,
          PrefetchContainer::Key(
              std::optional<blink::DocumentToken>(std::nullopt),
              url),
          prefetch_type,
          embedder_histogram_suffix,
          referrer,
          /*speculation_rules_tags=*/std::nullopt,
          std::move(no_vary_search_hint),
          /*prefetch_document_manager=*/nullptr,
          referring_web_contents.GetBrowserContext()->GetWeakPtr(),
          ukm::kInvalidSourceId,
          std::move(preload_pipeline_info),
          std::move(attempt),
          holdback_status_override,
          /*initiator_devtools_navigation_token=*/std::nullopt,
          /*Must be empty: additional_headers=*/{},
          /*request_status_listener=*/nullptr,
          referring_web_contents.GetOrCreateWebPreferences().javascript_enabled,
          PrefetchContainerDefaultTtlInPrefetchService(),
          /*should_append_variations_header=*/true) {
  CHECK(!prefetch_type_.IsRendererInitiated());
  CHECK(PrefetchBrowserInitiatedTriggersEnabled());
  CHECK(!embedder_histogram_suffix_.value().empty());
}

PrefetchContainer::PrefetchContainer(
    BrowserContext* browser_context,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const std::string& embedder_histogram_suffix,
    const blink::mojom::Referrer& referrer,
    bool javascript_enabled,
    const std::optional<url::Origin>& referring_origin,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    base::WeakPtr<PreloadingAttempt> attempt,
    const net::HttpRequestHeaders& additional_headers,
    std::unique_ptr<PrefetchRequestStatusListener> request_status_listener,
    base::TimeDelta ttl_in_sec,
    bool should_append_variations_header)
    : PrefetchContainer(
          GlobalRenderFrameHostId(),
          referring_origin,
          /*referring_url_hash=*/std::nullopt,
          PrefetchContainer::Key(
              std::optional<blink::DocumentToken>(std::nullopt),
              url),
          prefetch_type,
          embedder_histogram_suffix,
          referrer,
          /*speculation_rules_tags=*/std::nullopt,
          std::move(no_vary_search_hint),
          /*prefetch_document_manager=*/nullptr,
          browser_context->GetWeakPtr(),
          ukm::kInvalidSourceId,
          PreloadPipelineInfo::Create(
              /*planned_max_preloading_type=*/PreloadingType::kPrefetch),
          std::move(attempt),
          /*holdback_status_override=*/std::nullopt,
          /*initiator_devtools_navigation_token=*/std::nullopt,
          additional_headers,
          std::move(request_status_listener),
          javascript_enabled,
          ttl_in_sec,
          should_append_variations_header) {
  CHECK(!prefetch_type_.IsRendererInitiated());
  CHECK(PrefetchBrowserInitiatedTriggersEnabled());
  CHECK(!embedder_histogram_suffix_.value().empty());
}

PrefetchContainer::PrefetchContainer(
    const GlobalRenderFrameHostId& referring_render_frame_host_id,
    const std::optional<url::Origin>& referring_origin,
    const std::optional<size_t>& referring_url_hash,
    const PrefetchContainer::Key& key,
    const PrefetchType& prefetch_type,
    const std::optional<std::string>& embedder_histogram_suffix,
    const blink::mojom::Referrer& referrer,
    std::optional<SpeculationRulesTags> speculation_rules_tags,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager,
    base::WeakPtr<BrowserContext> browser_context,
    ukm::SourceId ukm_source_id,
    scoped_refptr<PreloadPipelineInfo> preload_pipeline_info,
    base::WeakPtr<PreloadingAttempt> attempt,
    std::optional<PreloadingHoldbackStatus> holdback_status_override,
    std::optional<base::UnguessableToken> initiator_devtools_navigation_token,
    const net::HttpRequestHeaders& additional_headers,
    std::unique_ptr<PrefetchRequestStatusListener> request_status_listener,
    bool is_javascript_enabled,
    base::TimeDelta ttl_in_sec,
    bool should_append_variations_header)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      referring_origin_(referring_origin),
      referring_url_hash_(referring_url_hash),
      key_(key),
      prefetch_type_(prefetch_type),
      embedder_histogram_suffix_(embedder_histogram_suffix),
      referrer_(referrer),
      no_vary_search_hint_(std::move(no_vary_search_hint)),
      speculation_rules_tags_(std::move(speculation_rules_tags)),
      prefetch_document_manager_(std::move(prefetch_document_manager)),
      browser_context_(std::move(browser_context)),
      ukm_source_id_(ukm_source_id),
      request_id_(base::UnguessableToken::Create().ToString()),
      preload_pipeline_info_(base::WrapRefCounted(
          static_cast<PreloadPipelineInfoImpl*>(preload_pipeline_info.get()))),
      attempt_(std::move(attempt)),
      holdback_status_override_(holdback_status_override),
      initiator_devtools_navigation_token_(
          std::move(initiator_devtools_navigation_token)),
      additional_headers_(additional_headers),
      request_status_listener_(std::move(request_status_listener)),
      is_javascript_enabled_(is_javascript_enabled),
      ttl_in_sec_(ttl_in_sec),
      should_append_variations_header_(should_append_variations_header) {
  is_likely_ahead_of_prerender_ =
      CalculateIsLikelyAheadOfPrerender(*preload_pipeline_info_);

  const bool is_reusable = [&]() -> bool {
    if (base::FeatureList::IsEnabled(features::kPrefetchReusable)) {
      return true;
    }

    // If `kPrerender2FallbackPrefetchSpecRules` is enabled, SpecRules prerender
    // triggers a prefetch ahead of prerender. If prerender failed after initial
    // navigation (with prefetch), e.g. due to use of forbidden mojo interface
    // in prerendering, the following user-initiated navigation reaches here. We
    // allow multiple use of the result of such prefetch to prevent the second
    // fetch via network.
    //
    // Note that this logic reduces the second fetch iff the prefetch is
    // ahead of prerender and doesn't for a prefetch that is not ahead of
    // prerender and then marked as `IsLikelyAheadOfPrerender()`. This is
    // because we can't update the property of `PrefetchResponseReader` after
    // ctor.
    //
    // TODO(crbug.com/40064891): Remove this once `kPrefetchReusable` is
    // launched.
    //
    // Note that we keep the check instead of
    // `features::UsePrefetchPrerenderIntegration()` as `PrefetchReusable` is
    // enabled on Desktop and the difference doesn't affect `SearchPreload2`.
    if (base::FeatureList::IsEnabled(
            features::kPrerender2FallbackPrefetchSpecRules)) {
      switch (features::kPrerender2FallbackPrefetchReusablePolicy.Get()) {
        case features::Prerender2FallbackPrefetchReusablePolicy::kNotUse:
          return false;
        case features::Prerender2FallbackPrefetchReusablePolicy::
            kUseIfIsLikelyAheadOfPrerender:
          return is_likely_ahead_of_prerender_;
        case features::Prerender2FallbackPrefetchReusablePolicy::kUseAlways:
          return true;
      }
    }

    return false;
  }();
  redirect_chain_.push_back(std::make_unique<SinglePrefetch>(
      GetURL(), IsCrossSiteRequest(url::Origin::Create(GetURL())),
      is_reusable));

  // Disallow prefetching ServiceWorker-controlled responses for isolated
  // network contexts.
  if (!features::IsPrefetchServiceWorkerEnabled(browser_context_.get()) ||
      IsIsolatedNetworkContextRequiredForCurrentPrefetch()) {
    service_worker_state_ = PrefetchServiceWorkerState::kDisallowed;
  }
}

PrefetchContainer::~PrefetchContainer() {
  is_in_dtor_ = true;

  // Ideally, this method should be called just before dtor.
  // https://chromium-review.googlesource.com/c/chromium/src/+/5657659/comments/0cfb14c0_3050963e
  //
  // TODO(crbug.com/356314759): Do it.
  OnWillBeDestroyed();

  CancelStreamingURLLoaderIfNotServing();

  MaybeRecordPrefetchStatusToUMA(
      prefetch_status_.value_or(PrefetchStatus::kPrefetchNotStarted));
  RecordDurationFromAdded();

  ukm::builders::PrefetchProxy_PrefetchedResource builder(ukm_source_id_);
  builder.SetResourceType(/*mainframe*/ 1);
  builder.SetStatus(static_cast<int>(
      prefetch_status_.value_or(PrefetchStatus::kPrefetchNotStarted)));
  builder.SetLinkClicked(navigated_to_);

  if (prefetch_response_sizes_) {
    builder.SetDataLength(ukm::GetExponentialBucketMinForBytes(
        prefetch_response_sizes_->encoded_data_length));
  }

  if (fetch_duration_) {
    builder.SetFetchDurationMS(fetch_duration_->InMilliseconds());
  }

  if (probe_result_) {
    builder.SetISPFilteringStatus(static_cast<int>(probe_result_.value()));
  }

  // TODO(crbug.com/40215782): Get the navigation start time and set the
  // NavigationStartToFetchStartMs field of the PrefetchProxy.PrefetchedResource
  // UKM event.

  builder.Record(ukm::UkmRecorder::Get());

  if (prefetch_document_manager_) {
    prefetch_document_manager_->PrefetchWillBeDestroyed(this);
  }
}

void PrefetchContainer::OnWillBeDestroyed() {
  for (auto& observer : observers_) {
    observer.OnWillBeDestroyed(*this);
  }
}

PrefetchContainer::Key::Key(
    std::optional<blink::DocumentToken> referring_document_token,
    GURL url)
    : referring_document_token_or_nik_(std::move(referring_document_token)),
      url_(std::move(url)) {
  CHECK(!PrefetchNIKScopeEnabled());
}

PrefetchContainer::Key::Key(
    net::NetworkIsolationKey referring_network_isolation_key,
    GURL url)
    : referring_document_token_or_nik_(
          std::move(referring_network_isolation_key)),
      url_(std::move(url)) {
  CHECK(PrefetchNIKScopeEnabled());
}

PrefetchContainer::Key::~Key() = default;

PrefetchContainer::Key::Key(PrefetchContainer::Key&& other) = default;

PrefetchContainer::Key& PrefetchContainer::Key::operator=(
    PrefetchContainer::Key&& other) = default;

PrefetchContainer::Key::Key(const PrefetchContainer::Key& other) = default;

PrefetchContainer::Key& PrefetchContainer::Key::operator=(
    const PrefetchContainer::Key& other) = default;

PrefetchContainer::Reader::Reader() : Reader(nullptr, 0) {}

PrefetchContainer::Reader::Reader(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    size_t index_redirect_chain_to_serve)
    : prefetch_container_(std::move(prefetch_container)),
      index_redirect_chain_to_serve_(index_redirect_chain_to_serve) {}

PrefetchContainer::Reader::Reader(Reader&&) = default;
PrefetchContainer::Reader& PrefetchContainer::Reader::operator=(Reader&&) =
    default;
PrefetchContainer::Reader::~Reader() = default;

PrefetchContainer::Reader PrefetchContainer::Reader::Clone() const {
  return Reader(prefetch_container_, index_redirect_chain_to_serve_);
}

PrefetchContainer::Reader PrefetchContainer::CreateReader() {
  return Reader(GetWeakPtr(), 0);
}

// Please follow go/preloading-dashboard-updates if a new outcome enum or a
// failure reason enum is added.
void PrefetchContainer::SetTriggeringOutcomeAndFailureReasonFromStatus(
    PrefetchStatus new_prefetch_status) {
  std::optional<PrefetchStatus> old_prefetch_status = prefetch_status_;
  if (old_prefetch_status &&
      old_prefetch_status.value() == PrefetchStatus::kPrefetchResponseUsed) {
    // Skip this update if the triggering outcome has already been updated
    // to kSuccess.
    return;
  }

  if (old_prefetch_status &&
      (TriggeringOutcomeFromStatus(old_prefetch_status.value()) ==
       PreloadingTriggeringOutcome::kFailure)) {
    if (StatusUpdateIsPossibleAfterFailure(new_prefetch_status)) {
      // Note that `StatusUpdateIsPossibleAfterFailure()` implies that
      // the new status is a failure.
      CHECK(TriggeringOutcomeFromStatus(new_prefetch_status) ==
            PreloadingTriggeringOutcome::kFailure);

      // Skip this update since if the triggering outcome has already been
      // updated to kFailure, we don't need to overwrite it.
      return;
    } else {
      SCOPED_CRASH_KEY_NUMBER("PrefetchContainer", "prefetch_status_from",
                              static_cast<int>(old_prefetch_status.value()));
      SCOPED_CRASH_KEY_NUMBER("PrefetchContainer", "prefetch_status_to",
                              static_cast<int>(new_prefetch_status));
      NOTREACHED()
          << "PrefetchStatus illegal transition: (old_prefetch_status, "
             "new_prefetch_status) = ("
          << static_cast<int>(old_prefetch_status.value()) << ", "
          << static_cast<int>(new_prefetch_status) << ")";
    }
  }

  // We record the prefetch status to UMA if it's a failure, or if the prefetch
  // response is being used. For other statuses, there may be more updates in
  // the future, so we only record them in the destructor.
  // Note: The prefetch may have an updated failure status in the future
  // (for example: if the triggering speculation candidate for a failed prefetch
  // is removed), but the original failure is more pertinent for metrics
  // purposes.
  if (TriggeringOutcomeFromStatus(new_prefetch_status) ==
          PreloadingTriggeringOutcome::kFailure ||
      new_prefetch_status == PrefetchStatus::kPrefetchResponseUsed) {
    MaybeRecordPrefetchStatusToUMA(new_prefetch_status);
  }

  if (attempt_) {
    switch (new_prefetch_status) {
      case PrefetchStatus::kPrefetchNotFinishedInTime:
        attempt_->SetTriggeringOutcome(PreloadingTriggeringOutcome::kRunning);
        break;
      case PrefetchStatus::kPrefetchSuccessful:
        // A successful prefetch means the response is ready to be used for the
        // next navigation.
        attempt_->SetTriggeringOutcome(PreloadingTriggeringOutcome::kReady);
        break;
      case PrefetchStatus::kPrefetchResponseUsed:
        if (old_prefetch_status && old_prefetch_status.value() !=
                                       PrefetchStatus::kPrefetchSuccessful) {
          // If the new prefetch status is |kPrefetchResponseUsed| or
          // |kPrefetchUsedNoProbe| but the previous status is not
          // |kPrefetchSuccessful|, then temporarily update the triggering
          // outcome to |kReady| to ensure valid triggering outcome state
          // transitions. This can occur in cases where the prefetch is served
          // before the body is fully received.
          attempt_->SetTriggeringOutcome(PreloadingTriggeringOutcome::kReady);
        }
        attempt_->SetTriggeringOutcome(PreloadingTriggeringOutcome::kSuccess);
        break;
      // A decoy is considered eligible because a network request is made for
      // it. It is considered as a failure as the final response is never
      // served.
      case PrefetchStatus::kPrefetchIsPrivacyDecoy:
      case PrefetchStatus::kPrefetchFailedNetError:
      case PrefetchStatus::kPrefetchFailedNon2XX:
      case PrefetchStatus::kPrefetchFailedMIMENotSupported:
      case PrefetchStatus::kPrefetchFailedInvalidRedirect:
      case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
      case PrefetchStatus::kPrefetchNotUsedProbeFailed:
      case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
      // TODO(adithyas): This would report 'eviction' as a failure even though
      // the initial prefetch succeeded, consider introducing a different
      // PreloadingTriggerOutcome for eviction.
      case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
      case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
      case PrefetchStatus::kPrefetchIsStale:
      case PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved:
        attempt_->SetFailureReason(
            ToPreloadingFailureReason(new_prefetch_status));
        break;
      case PrefetchStatus::kPrefetchHeldback:
      case PrefetchStatus::kPrefetchNotStarted:
        // `kPrefetchNotStarted` is set in
        // `PrefetchService::OnGotEligibilityForNonRedirect()` when the
        // container is pushed onto the prefetch queue, which occurs before the
        // holdback status is determined in
        // `PrefetchService::StartSinglePrefetch`. After the container is queued
        // and before it is sent for prefetch, the only status change is when
        // the container is popped from the queue but heldback. This is covered
        // by attempt's holdback status. For these two reasons this
        // PrefetchStatus does not fire a `SetTriggeringOutcome`.
        break;
      case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
      case PrefetchStatus::
          kPrefetchIneligibleUserHasServiceWorkerNoFetchHandler:
      case PrefetchStatus::kPrefetchIneligibleRedirectFromServiceWorker:
      case PrefetchStatus::kPrefetchIneligibleRedirectToServiceWorker:
      case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
      case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
      case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
      case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
      case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
      case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
      case PrefetchStatus::kPrefetchIneligibleExistingProxy:
      case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
      case PrefetchStatus::kPrefetchIneligibleRetryAfter:
      case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
      case PrefetchStatus::
          kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
        NOTIMPLEMENTED();
    }
  }
}

void PrefetchContainer::SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
    PrefetchStatus prefetch_status) {
  prefetch_status_ = prefetch_status;
  preload_pipeline_info_->SetPrefetchStatus(prefetch_status);
  for (auto& preload_pipeline_info : inherited_preload_pipeline_infos_) {
    preload_pipeline_info->SetPrefetchStatus(prefetch_status);
  }

  // Currently DevTools only supports when the prefetch is initiated by
  // renderer.
  if (IsRendererInitiated()) {
    FrameTreeNode* ftn = FrameTreeNode::From(
        RenderFrameHostImpl::FromID(referring_render_frame_host_id_));

    std::optional<PreloadingTriggeringOutcome> preloading_trigger_outcome =
        TriggeringOutcomeFromStatus(prefetch_status);

    if (initiator_devtools_navigation_token_.has_value() &&
        preloading_trigger_outcome.has_value()) {
      devtools_instrumentation::DidUpdatePrefetchStatus(
          ftn, initiator_devtools_navigation_token_.value(), GetURL(),
          preload_pipeline_info_->id(), preloading_trigger_outcome.value(),
          prefetch_status, RequestId());
    }
  }
}

void PrefetchContainer::SetPrefetchStatus(PrefetchStatus prefetch_status) {
  // The concept of `PreloadingAttempt`'s `PreloadingTriggeringOutcome` is to
  // record the outcomes of started triggers. Therefore, this should
  // only be called once prefetching has actually started, and not for
  // ineligible or eligibled but not started triggers (e.g., holdback triggers,
  // triggers waiting on a queue).
  if (GetLoadState() == LoadState::kStarted) {
    SetTriggeringOutcomeAndFailureReasonFromStatus(prefetch_status);
  }
  SetPrefetchStatusWithoutUpdatingTriggeringOutcome(prefetch_status);
}

PrefetchStatus PrefetchContainer::GetPrefetchStatus() const {
  DCHECK(prefetch_status_);
  return prefetch_status_.value();
}

void PrefetchContainer::TakeProxyLookupClient(
    std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client) {
  DCHECK(!proxy_lookup_client_);
  proxy_lookup_client_ = std::move(proxy_lookup_client);
}

std::unique_ptr<ProxyLookupClientImpl>
PrefetchContainer::ReleaseProxyLookupClient() {
  DCHECK(proxy_lookup_client_);
  return std::move(proxy_lookup_client_);
}

PrefetchNetworkContext*
PrefetchContainer::GetOrCreateNetworkContextForCurrentPrefetch() {
  bool is_isolated_network_context_required =
      IsIsolatedNetworkContextRequiredForCurrentPrefetch();

  auto network_context_itr =
      network_contexts_.find(is_isolated_network_context_required);
  if (network_context_itr == network_contexts_.end()) {
    network_context_itr =
        network_contexts_
            .emplace(is_isolated_network_context_required,
                     std::make_unique<PrefetchNetworkContext>(
                         is_isolated_network_context_required, prefetch_type_,
                         referring_render_frame_host_id_, referring_origin_))
            .first;
  }

  CHECK(network_context_itr != network_contexts_.end());
  CHECK(network_context_itr->second);
  return network_context_itr->second.get();
}

PrefetchNetworkContext*
PrefetchContainer::Reader::GetCurrentNetworkContextToServe() const {
  const SinglePrefetch& this_prefetch = GetCurrentSinglePrefetchToServe();

  const auto& network_context_itr = prefetch_container_->network_contexts_.find(
      this_prefetch.is_isolated_network_context_required_);
  if (network_context_itr == prefetch_container_->network_contexts_.end()) {
    // Not set in unit tests.
    return nullptr;
  }
  return network_context_itr->second.get();
}

void PrefetchContainer::CloseIdleConnections() {
  for (const auto& network_context_itr : network_contexts_) {
    CHECK(network_context_itr.second);
    network_context_itr.second->CloseIdleConnections();
  }
}

PrefetchDocumentManager* PrefetchContainer::GetPrefetchDocumentManager() const {
  return prefetch_document_manager_.get();
}

void PrefetchContainer::SetLoadState(LoadState new_load_state) {
  switch (new_load_state) {
    case LoadState::kNotStarted:
      NOTREACHED();

    case LoadState::kEligible:
    case LoadState::kFailedIneligible:
      CHECK_EQ(load_state_, LoadState::kNotStarted);
      break;

    case LoadState::kStarted:
    case LoadState::kFailedHeldback:
      CHECK_EQ(load_state_, LoadState::kEligible);
      break;
  }
  DVLOG(1) << (*this) << " LoadState " << load_state_ << " -> "
           << new_load_state;
  load_state_ = new_load_state;
}

PrefetchContainer::LoadState PrefetchContainer::GetLoadState() const {
  return load_state_;
}

void PrefetchContainer::OnAddedToPrefetchService() {
  time_added_to_prefetch_service_ = base::TimeTicks::Now();
}

void PrefetchContainer::OnEligibilityCheckComplete(
    PreloadingEligibility eligibility) {
  SinglePrefetch& this_prefetch = GetCurrentSinglePrefetchToPrefetch();
  this_prefetch.eligibility_ = eligibility;
  preload_pipeline_info_->SetPrefetchEligibility(eligibility);
  for (auto& preload_pipeline_info : inherited_preload_pipeline_infos_) {
    preload_pipeline_info->SetPrefetchEligibility(eligibility);
  }

  bool is_eligible = eligibility == PreloadingEligibility::kEligible;

  if (redirect_chain_.size() == 1) {
    // This case is for just the URL that was originally requested to be
    // prefetched.
    if (is_eligible) {
      SetLoadState(LoadState::kEligible);
    } else {
      SetLoadState(LoadState::kFailedIneligible);
      PrefetchStatus new_prefetch_status =
          PrefetchStatusFromIneligibleReason(eligibility);
      MaybeRecordPrefetchStatusToUMA(new_prefetch_status);
      SetPrefetchStatusWithoutUpdatingTriggeringOutcome(new_prefetch_status);
      OnInitialPrefetchFailedIneligible(eligibility);
    }

    if (attempt_) {
      // Please follow go/preloading-dashboard-updates if a new eligibility is
      // added.
      attempt_->SetEligibility(eligibility);
    }

    time_initial_eligibility_got_ = base::TimeTicks::Now();

    // Recording an eligiblity for PrefetchReferringPageMetrics.
    // TODO(crbug.com/40946257): Current code doesn't support
    // PrefetchReferringPageMetrics when the prefetch is initiated by browser.
    if (IsRendererInitiated()) {
      if (prefetch_document_manager_) {
        prefetch_document_manager_->OnEligibilityCheckComplete(is_eligible);
      }
    }

    for (auto& observer : observers_) {
      observer.OnGotInitialEligibility(*this, eligibility);
    }
  } else {
    // This case is for any URLs from redirects.
    if (!is_eligible) {
      SetPrefetchStatus(PrefetchStatus::kPrefetchFailedIneligibleRedirect);
    }
  }
}

bool PrefetchContainer::IsInitialPrefetchEligible() const {
  DCHECK(redirect_chain_.size() > 0);
  return redirect_chain_[0]->eligibility_ &&
         redirect_chain_[0]->eligibility_.value() ==
             PreloadingEligibility::kEligible;
}

void PrefetchContainer::AddRedirectHop(const net::RedirectInfo& redirect_info) {
  CHECK(resource_request_);

  // There are sometimes other headers that are modified during navigation
  // redirects; see |NavigationRequest::OnRedirectChecksComplete| (including
  // some which are added by throttles). These aren't yet supported for
  // prefetch, including browsing topics and client hints.
  net::HttpRequestHeaders updated_headers;
  std::vector<std::string> headers_to_remove = {variations::kClientDataHeader};
  updated_headers.SetHeader(blink::kSecPurposeHeaderName,
                            GetSecPurposeHeaderValue(redirect_info.new_url));

  // Remove any existing client hints headers (except, below, if we still want
  // to send this particular hint).
  if (base::FeatureList::IsEnabled(features::kPrefetchClientHints)) {
    const auto& client_hints = network::GetClientHintToNameMap();
    headers_to_remove.reserve(headers_to_remove.size() + client_hints.size());
    for (const auto& [_, header] : client_hints) {
      headers_to_remove.push_back(header);
    }
  }

  // Sec-Speculation-Tags is set only when the prefetch is triggered
  // by speculation rules and it is not cross-site prefetch redirection.
  // To see more details:
  // https://github.com/WICG/nav-speculation/blob/main/speculation-rules-tags.md#the-cross-site-case
  headers_to_remove.push_back(blink::kSecSpeculationTagsHeaderName);
  if (speculation_rules_tags_.has_value() &&
      !IsCrossSiteRequest(url::Origin::Create(redirect_info.new_url))) {
    CHECK(IsSpeculationRuleType(prefetch_type_.trigger_type()));
    std::optional<std::string> serialized_list =
        speculation_rules_tags_->ConvertStringToHeaderString();
    CHECK(serialized_list.has_value());
    updated_headers.SetHeader(blink::kSecSpeculationTagsHeaderName,
                              serialized_list.value());
  }

  // Then add the client hints that are appropriate for the redirect.
  AddClientHintsHeaders(url::Origin::Create(redirect_info.new_url),
                        &updated_headers);

  // To avoid spurious reordering, don't remove headers that will be updated
  // anyway.
  std::erase_if(headers_to_remove, [&](const std::string& header) {
    return updated_headers.HasHeader(header);
  });

  // TODO(jbroman): We have several places that invoke
  // `net::RedirectUtil::UpdateHttpRequest` and then need to do very similar
  // work afterward. Ideally we would deduplicate these more.
  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      resource_request_->url, resource_request_->method, redirect_info,
      std::move(headers_to_remove), std::move(updated_headers),
      &resource_request_->headers, &should_clear_upload);
  CHECK(!should_clear_upload);

  resource_request_->url = redirect_info.new_url;
  resource_request_->method = redirect_info.new_method;
  resource_request_->site_for_cookies = redirect_info.new_site_for_cookies;

  resource_request_->trusted_params->isolation_info =
      resource_request_->trusted_params->isolation_info.CreateForRedirect(
          url::Origin::Create(resource_request_->url));

  // TODO(jbroman): This somewhat duplicates |referrer_|. Revisit usage of that
  // (and related data members) to see if they can/should use this data instead.
  resource_request_->referrer = GURL(redirect_info.new_referrer);
  resource_request_->referrer_policy = redirect_info.new_referrer_policy;

  AddXClientDataHeader(*resource_request_.get());

  redirect_chain_.push_back(std::make_unique<SinglePrefetch>(
      redirect_info.new_url,
      IsCrossSiteRequest(url::Origin::Create(redirect_info.new_url)),
      // If `PrefetchResponseReader` of the initial navigation is reusable,
      // inherit the property.
      redirect_chain_[0]->response_reader_->is_reusable()));
}

bool PrefetchContainer::IsCrossSiteRequest(const url::Origin& origin) const {
  return referring_origin_.has_value() &&
         !net::SchemefulSite::IsSameSite(referring_origin_.value(), origin);
}

bool PrefetchContainer::IsCrossOriginRequest(const url::Origin& origin) const {
  return referring_origin_.has_value() &&
         !referring_origin_.value().IsSameOriginWith(origin);
}

void PrefetchContainer::MarkCrossSiteContaminated() {
  is_cross_site_contaminated_ = true;
}

void PrefetchContainer::AddXClientDataHeader(
    network::ResourceRequest& request) {
  if (browser_context_) {
    // Add X-Client-Data header with experiment IDs from field trials.
    variations::AppendVariationsHeader(request.url,
                                       browser_context_->IsOffTheRecord()
                                           ? variations::InIncognito::kYes
                                           : variations::InIncognito::kNo,
                                       variations::SignedIn::kNo, &request);
  }
}

void PrefetchContainer::RegisterCookieListener(
    network::mojom::CookieManager* cookie_manager) {
  SinglePrefetch& this_prefetch = GetCurrentSinglePrefetchToPrefetch();
  this_prefetch.cookie_listener_ = PrefetchCookieListener::MakeAndRegister(
      this_prefetch.url_, cookie_manager);
}

void PrefetchContainer::PauseAllCookieListeners() {
  // TODO(crbug.com/377440445): Consider whether we actually need to
  // pause/resume all single prefetch's cookie listener during each single
  // prefetch's isolated cookie copy.
  for (const auto& single_prefetch : redirect_chain_) {
    if (single_prefetch->cookie_listener_) {
      single_prefetch->cookie_listener_->PauseListening();
    }
  }
}

void PrefetchContainer::ResumeAllCookieListeners() {
  // TODO(crbug.com/377440445): Consider whether we actually need to
  // pause/resume all single prefetch's cookie listener during each single
  // prefetch's isolated cookie copy.
  for (const auto& single_prefetch : redirect_chain_) {
    if (single_prefetch->cookie_listener_) {
      single_prefetch->cookie_listener_->ResumeListening();
    }
  }
}

bool PrefetchContainer::Reader::HaveDefaultContextCookiesChanged() const {
  const SinglePrefetch& this_prefetch = GetCurrentSinglePrefetchToServe();
  if (this_prefetch.cookie_listener_) {
    return this_prefetch.cookie_listener_->HaveCookiesChanged();
  }
  return false;
}

bool PrefetchContainer::Reader::HasIsolatedCookieCopyStarted() const {
  switch (GetCurrentSinglePrefetchToServe().cookie_copy_status_) {
    case SinglePrefetch::CookieCopyStatus::kNotStarted:
      return false;
    case SinglePrefetch::CookieCopyStatus::kInProgress:
    case SinglePrefetch::CookieCopyStatus::kCompleted:
      return true;
  }
}

bool PrefetchContainer::Reader::IsIsolatedCookieCopyInProgress() const {
  switch (GetCurrentSinglePrefetchToServe().cookie_copy_status_) {
    case SinglePrefetch::CookieCopyStatus::kNotStarted:
    case SinglePrefetch::CookieCopyStatus::kCompleted:
      return false;
    case SinglePrefetch::CookieCopyStatus::kInProgress:
      return true;
  }
}

void PrefetchContainer::Reader::OnIsolatedCookieCopyStart() const {
  DCHECK(!IsIsolatedCookieCopyInProgress());

  // We should temporarily ignore the cookie monitoring by
  // `PrefetchCookieListener` during the isolated cookie is written to the
  // default network context.
  // `PrefetchCookieListener` should monitor whether the cookie is changed from
  // what we stored in isolated network context when prefetching so that we can
  // avoid serving the stale prefetched content. Currently
  // `PrefetchCookieListener` will also catch isolated cookie copy as a cookie
  // change. To handle this event as a false positive (as the cookie isn't
  // changed from what we stored on prefetching), we can pause the lisner during
  // copying, keeping the prefetch servable.
  prefetch_container_->PauseAllCookieListeners();

  GetCurrentSinglePrefetchToServe().cookie_copy_status_ =
      SinglePrefetch::CookieCopyStatus::kInProgress;

  GetCurrentSinglePrefetchToServe().cookie_copy_start_time_ =
      base::TimeTicks::Now();
}

void PrefetchContainer::Reader::OnIsolatedCookiesReadCompleteAndWriteStart()
    const {
  DCHECK(IsIsolatedCookieCopyInProgress());

  GetCurrentSinglePrefetchToServe().cookie_read_end_and_write_start_time_ =
      base::TimeTicks::Now();
}

void PrefetchContainer::Reader::OnIsolatedCookieCopyComplete() const {
  DCHECK(IsIsolatedCookieCopyInProgress());

  // Resumes `PrefetchCookieListener` so that we can keep monitoring the
  // cookie change for the prefetch, which may be served again.
  prefetch_container_->ResumeAllCookieListeners();

  const auto& this_prefetch = GetCurrentSinglePrefetchToServe();

  this_prefetch.cookie_copy_status_ =
      SinglePrefetch::CookieCopyStatus::kCompleted;

  if (this_prefetch.cookie_copy_start_time_.has_value() &&
      this_prefetch.cookie_read_end_and_write_start_time_.has_value()) {
    RecordCookieCopyTimes(
        this_prefetch.cookie_copy_start_time_.value(),
        this_prefetch.cookie_read_end_and_write_start_time_.value(),
        base::TimeTicks::Now());
  }

  if (this_prefetch.on_cookie_copy_complete_callback_) {
    std::move(this_prefetch.on_cookie_copy_complete_callback_).Run();
  }
}

void PrefetchContainer::Reader::OnInterceptorCheckCookieCopy() const {
  if (!GetCurrentSinglePrefetchToServe().cookie_copy_start_time_) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      base::TimeTicks::Now() -
          GetCurrentSinglePrefetchToServe().cookie_copy_start_time_.value(),
      base::TimeDelta(), base::Seconds(5), 50);
}

void PrefetchContainer::Reader::SetOnCookieCopyCompleteCallback(
    base::OnceClosure callback) const {
  DCHECK(IsIsolatedCookieCopyInProgress());

  GetCurrentSinglePrefetchToServe().on_cookie_copy_complete_callback_ =
      std::move(callback);
}

void PrefetchContainer::SetStreamingURLLoader(
    base::WeakPtr<PrefetchStreamingURLLoader> streaming_loader) {
  // The previous streaming loader (if any) should be already deleted or to be
  // deleted soon when the new `streaming_loader` is set here.
  CHECK(!streaming_loader_ || streaming_loader_->IsDeletionScheduledForCHECK());

  streaming_loader_ = std::move(streaming_loader);
}

base::WeakPtr<PrefetchStreamingURLLoader>
PrefetchContainer::GetStreamingURLLoader() const {
  // Streaming loaders already deleted or scheduled to be deleted shouldn't be
  // used.
  if (!streaming_loader_ || streaming_loader_->IsDeletionScheduledForCHECK()) {
    return nullptr;
  }
  return streaming_loader_;
}

bool PrefetchContainer::IsStreamingURLLoaderDeletionScheduledForTesting()
    const {
  return streaming_loader_ && streaming_loader_->IsDeletionScheduledForCHECK();
}

const PrefetchResponseReader* PrefetchContainer::GetNonRedirectResponseReader()
    const {
  CHECK(!redirect_chain_.empty());
  if (!redirect_chain_.back()->response_reader_->GetHead()) {
    // Either the last PrefetchResponseReader is for a redirect response, or for
    // a final response not yet receiving its header.
    return nullptr;
  }
  return redirect_chain_.back()->response_reader_.get();
}

const network::mojom::URLResponseHead* PrefetchContainer::GetNonRedirectHead()
    const {
  return GetNonRedirectResponseReader()
             ? GetNonRedirectResponseReader()->GetHead()
             : nullptr;
}

std::pair<PrefetchRequestHandler, base::WeakPtr<ServiceWorkerClient>>
PrefetchContainer::Reader::CreateRequestHandler() {
  // Create a `PrefetchRequestHandler` from the current `SinglePrefetch` (==
  // `reader`) and its corresponding `PrefetchStreamingURLLoader`.
  auto handler = GetCurrentSinglePrefetchToServe()
                     .response_reader_->CreateRequestHandler();

  // Advance the current `SinglePrefetch` position.
  AdvanceCurrentURLToServe();

  return handler;
}

bool PrefetchContainer::Reader::VariesOnCookieIndices() const {
  return GetCurrentSinglePrefetchToServe()
      .response_reader_->VariesOnCookieIndices();
}

bool PrefetchContainer::Reader::MatchesCookieIndices(
    base::span<const std::pair<std::string, std::string>> cookies) const {
  return GetCurrentSinglePrefetchToServe()
      .response_reader_->MatchesCookieIndices(cookies);
}

void PrefetchContainer::CancelStreamingURLLoaderIfNotServing() {
  if (!streaming_loader_) {
    return;
  }
  streaming_loader_->CancelIfNotServing();
  streaming_loader_.reset();
}

void PrefetchContainer::Reader::OnPrefetchProbeResult(
    PrefetchProbeResult probe_result) const {
  prefetch_container_->probe_result_ = probe_result;

  // It's possible for the prefetch to fail (e.g., due to a network error) while
  // the origin probe is running. We avoid overwriting the status in that case.
  if (TriggeringOutcomeFromStatus(GetPrefetchStatus()) ==
      PreloadingTriggeringOutcome::kFailure) {
    return;
  }

  switch (probe_result) {
    case PrefetchProbeResult::kNoProbing:
    case PrefetchProbeResult::kDNSProbeSuccess:
    case PrefetchProbeResult::kTLSProbeSuccess:
      // Wait to update the prefetch status until the probe for the final
      // redirect hop is a success.
      if (index_redirect_chain_to_serve_ ==
          prefetch_container_->redirect_chain_.size() - 1) {
        prefetch_container_->SetPrefetchStatus(
            PrefetchStatus::kPrefetchResponseUsed);
      }
      break;
    case PrefetchProbeResult::kDNSProbeFailure:
    case PrefetchProbeResult::kTLSProbeFailure:
      prefetch_container_->SetPrefetchStatus(
          PrefetchStatus::kPrefetchNotUsedProbeFailed);
      break;
    default:
      NOTIMPLEMENTED();
  }
}

void PrefetchContainer::OnDeterminedHead() {
  if (GetNonRedirectHead()) {
    time_header_determined_successfully_ = base::TimeTicks::Now();
  }

  // Propagates the header to `no_vary_search_data_` if a non-redirect response
  // header is got.
  MaybeSetNoVarySearchData();

  for (auto& observer : observers_) {
    observer.OnDeterminedHead(*this);
  }
}

void PrefetchContainer::MaybeSetNoVarySearchData() {
  CHECK(!no_vary_search_data_.has_value());

  if (!GetNonRedirectHead()) {
    return;
  }

  // RenderFrameHostImpl will be used to display error messagse in DevTools
  // console. Can be null when the prefetch is browser-initiated.
  auto* rfhi_can_be_null =
      RenderFrameHostImpl::FromID(referring_render_frame_host_id_);
  no_vary_search_data_ = no_vary_search::ProcessHead(
      *GetNonRedirectHead(), GetURL(), rfhi_can_be_null);
}

void PrefetchContainer::StartTimeoutTimerIfNeeded(
    base::OnceClosure on_timeout_callback) {
  if (ttl_in_sec_.is_positive()) {
    CHECK(!timeout_timer_);
    timeout_timer_ = std::make_unique<base::OneShotTimer>();
    timeout_timer_->Start(FROM_HERE, ttl_in_sec_,
                          std::move(on_timeout_callback));
  }
}

// static
void PrefetchContainer::SetPrefetchResponseCompletedCallbackForTesting(
    PrefetchResponseCompletedCallbackForTesting callback) {
  GetPrefetchResponseCompletedCallbackForTesting() =  // IN-TEST
      std::move(callback);
}

void PrefetchContainer::OnPrefetchComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  DVLOG(1) << *this << "::OnPrefetchComplete";

  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.Prefetch.RedirectChainSize",
                           redirect_chain_.size());

  if (GetNonRedirectResponseReader()) {
    UpdatePrefetchRequestMetrics(
        GetNonRedirectResponseReader()->GetCompletionStatus(),
        GetNonRedirectResponseReader()->GetHead());
    UpdateServingPageMetrics();
  } else {
    DVLOG(1) << *this << "::OnPrefetchComplete:"
             << "no non redirect response reader";
  }

  if (IsDecoy()) {
    SetPrefetchStatus(PrefetchStatus::kPrefetchIsPrivacyDecoy);
    return;
  }

  // TODO(crbug.com/40250089): Call
  // `devtools_instrumentation::OnPrefetchBodyDataReceived()` with body of the
  // response.
  NotifyPrefetchRequestComplete(completion_status);

  int net_error = completion_status.error_code;
  int64_t body_length = completion_status.decoded_body_length;

  RecordPrefetchProxyPrefetchMainframeNetError(net_error);

  // Updates the prefetch's status if it hasn't been updated since the request
  // first started. For the prefetch to reach the network stack, it must have
  // `PrefetchStatus::kPrefetchNotStarted` or beyond.
  DCHECK(HasPrefetchStatus());
  if (GetPrefetchStatus() == PrefetchStatus::kPrefetchNotFinishedInTime) {
    SetPrefetchStatus(net_error == net::OK
                          ? PrefetchStatus::kPrefetchSuccessful
                          : PrefetchStatus::kPrefetchFailedNetError);
    UpdateServingPageMetrics();
  }

  if (net_error == net::OK) {
    time_prefetch_completed_successfully_ = base::TimeTicks::Now();
    RecordPrefetchProxyPrefetchMainframeBodyLength(body_length);
  }

  const PrefetchStatus prefetch_status = GetPrefetchStatus();
  if (prefetch_status == PrefetchStatus::kPrefetchSuccessful &&
      IsRendererInitiated()) {
    // TODO(crbug.com/40946257): Current code doesn't support
    // PrefetchReferringPageMetrics when the prefetch is initiated by browser.
    if (prefetch_document_manager_) {
      prefetch_document_manager_->OnPrefetchSuccessful(this);
    }
  }

  if (request_status_listener_) {
    switch (prefetch_status) {
      case PrefetchStatus::kPrefetchSuccessful:
      case PrefetchStatus::kPrefetchResponseUsed:
        request_status_listener_->OnPrefetchResponseCompleted();
        break;
      case PrefetchStatus::kPrefetchFailedNon2XX: {
        int response_code = GetNonRedirectHead()
                                ? GetNonRedirectHead()->headers->response_code()
                                : 0;
        request_status_listener_->OnPrefetchResponseServerError(response_code);
        break;
      }
      default:
        request_status_listener_->OnPrefetchResponseError();
        break;
    }
  }

  if (GetPrefetchResponseCompletedCallbackForTesting()) {
    GetPrefetchResponseCompletedCallbackForTesting().Run(  // IN-TEST
        GetWeakPtr());
  }
}

void PrefetchContainer::UpdatePrefetchRequestMetrics(
    const std::optional<network::URLLoaderCompletionStatus>& completion_status,
    const network::mojom::URLResponseHead* head) {
  DVLOG(1) << *this << "::UpdatePrefetchRequestMetrics:"
           << "head = " << head;
  if (completion_status) {
    prefetch_response_sizes_ = {
        .encoded_data_length = completion_status->encoded_data_length,
        .encoded_body_length = completion_status->encoded_body_length,
        .decoded_body_length = completion_status->decoded_body_length,
    };
  }

  if (head)
    header_latency_ =
        head->load_timing.receive_headers_end - head->load_timing.request_start;

  if (completion_status && head)
    fetch_duration_ =
        completion_status->completion_time - head->load_timing.request_start;
}

bool PrefetchContainer::HasPrefetchBeenConsideredToServe() const {
  // If `kPrefetchReusable` is enabled, we allow multiple navigations
  // to use a PrefetchContainer, and thus skip the `navigated_to_` check.
  if (base::FeatureList::IsEnabled(features::kPrefetchReusable)) {
    return false;
  }

  if (features::UsePrefetchPrerenderIntegration()) {
    // If `PrefetchResponseReader` of the initial navigation is reusable, it is
    // reusable.
    if (redirect_chain_[0]->response_reader_->is_reusable()) {
      return false;
    }
  }

  // Otherwise, if this prefetch has been considered to serve for a navigation
  // in the past, then it shouldn't be used for any future navigations.
  return navigated_to_;
}

PrefetchContainer::ServableState PrefetchContainer::GetServableState(
    base::TimeDelta cacheable_duration) const {
  // Servable if the non-redirect response (either fully or partially
  // received body) is servable.
  if (GetNonRedirectResponseReader() &&
      GetNonRedirectResponseReader()->Servable(cacheable_duration)) {
    return ServableState::kServable;
  }

  DVLOG(1) << *this << "(GetServableState)"
           << "(streaming_loader=" << GetStreamingURLLoader().get()
           << ", LoadState=" << load_state_ << ")";
  // Can only block until head if the request has been started using a
  // streaming URL loader and head/failure/redirect hasn't been received yet.
  if (GetStreamingURLLoader() &&
      redirect_chain_.back()->response_reader_->IsWaitingForResponse()) {
    return ServableState::kShouldBlockUntilHeadReceived;
  }

  if (features::UsePrefetchPrerenderIntegration()) {
    switch (load_state_) {
      case LoadState::kNotStarted:
      case LoadState::kEligible:
        return ServableState::kShouldBlockUntilEligibilityGot;
      case LoadState::kFailedIneligible:
      case LoadState::kStarted:
      case LoadState::kFailedHeldback:
        // nop
        break;
    }
  }

  return ServableState::kNotServable;
}

bool PrefetchContainer::Reader::DoesCurrentURLToServeMatch(
    const GURL& url) const {
  CHECK(index_redirect_chain_to_serve_ >= 1);
  return GetCurrentSinglePrefetchToServe().url_ == url;
}

PrefetchContainer::SinglePrefetch&
PrefetchContainer::GetCurrentSinglePrefetchToPrefetch() const {
  CHECK(redirect_chain_.size() > 0);
  return *redirect_chain_[redirect_chain_.size() - 1];
}

const PrefetchContainer::SinglePrefetch&
PrefetchContainer::GetPreviousSinglePrefetchToPrefetch() const {
  CHECK(redirect_chain_.size() > 1);
  return *redirect_chain_[redirect_chain_.size() - 2];
}

bool PrefetchContainer::Reader::IsEnd() const {
  CHECK(index_redirect_chain_to_serve_ <=
        prefetch_container_->redirect_chain_.size());
  return index_redirect_chain_to_serve_ >=
         prefetch_container_->redirect_chain_.size();
}

const PrefetchContainer::SinglePrefetch&
PrefetchContainer::Reader::GetCurrentSinglePrefetchToServe() const {
  CHECK(index_redirect_chain_to_serve_ >= 0 &&
        index_redirect_chain_to_serve_ <
            prefetch_container_->redirect_chain_.size());
  return *prefetch_container_->redirect_chain_[index_redirect_chain_to_serve_];
}

const GURL& PrefetchContainer::Reader::GetCurrentURLToServe() const {
  return GetCurrentSinglePrefetchToServe().url_;
}

void PrefetchContainer::SetServingPageMetrics(
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container) {
  serving_page_metrics_container_ = serving_page_metrics_container;
}

void PrefetchContainer::UpdateServingPageMetrics() {
  DVLOG(1) << *this << "::UpdateServingPageMetrics:"
           << "serving_page_metrics_container_ = "
           << serving_page_metrics_container_.get();
  if (!serving_page_metrics_container_) {
    return;
  }

  serving_page_metrics_container_->SetRequiredPrivatePrefetchProxy(
      GetPrefetchType().IsProxyRequiredWhenCrossOrigin());
  serving_page_metrics_container_->SetPrefetchHeaderLatency(
      GetPrefetchHeaderLatency());
  if (HasPrefetchStatus()) {
    serving_page_metrics_container_->SetPrefetchStatus(GetPrefetchStatus());
  }
}

void PrefetchContainer::SimulatePrefetchEligibleForTest() {
  if (attempt_) {
    attempt_->SetEligibility(PreloadingEligibility::kEligible);
    attempt_->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
  }
  SetLoadState(LoadState::kEligible);
  SetPrefetchStatus(PrefetchStatus::kPrefetchNotStarted);
}

void PrefetchContainer::SimulatePrefetchStartedForTest() {
  SetLoadState(LoadState::kStarted);
  SetPrefetchStatus(PrefetchStatus::kPrefetchNotFinishedInTime);
}

void PrefetchContainer::SimulatePrefetchCompletedForTest() {
  SetPrefetchStatus(PrefetchStatus::kPrefetchSuccessful);
}

void PrefetchContainer::SimulatePrefetchFailedIneligibleForTest(
    PreloadingEligibility eligibility) {
  CHECK_NE(PreloadingEligibility::kEligible, eligibility);

  if (attempt_) {
    attempt_->SetEligibility(eligibility);
  }
  SetLoadState(LoadState::kFailedIneligible);
}

void PrefetchContainer::OnDetectedCookiesChange(
    std::optional<bool>
        is_unblock_for_cookies_changed_triggered_by_this_prefetch_container) {
  // Multiple `PrefetchMatchResolver` can wait the same `PrefetchContainer`. So,
  // `OnDetectedCookiesChange()` can be called multiple times,
  if (on_detected_cookies_change_called_) {
    return;
  }
  on_detected_cookies_change_called_ = true;

  // There are cases that `prefetch_status_` is failure but this method is
  // called. For more details, see
  // https://docs.google.com/document/d/1G48SaWbdOy1yNBT1wio2IHVuUtddF5VLFsT6BRSYPMI/edit?tab=t.hpkotaxo7tfh#heading=h.woaoy8erwx63
  //
  // To prevent crash, we don't call `SetPrefetchStatus()`.
  if (prefetch_status_ &&
      TriggeringOutcomeFromStatus(prefetch_status_.value()) ==
          PreloadingTriggeringOutcome::kFailure) {
    SCOPED_CRASH_KEY_NUMBER("PrefetchContainer", "ODCC2_from",
                            static_cast<int>(prefetch_status_.value()));
    if (is_unblock_for_cookies_changed_triggered_by_this_prefetch_container
            .has_value()) {
      SCOPED_CRASH_KEY_BOOL(
          "PrefetchContainer", "ODCC2_iufcctbtpc",
          is_unblock_for_cookies_changed_triggered_by_this_prefetch_container
              .value());
    }
    base::debug::DumpWithoutCrashing();
    return;
  }

  CHECK_NE(GetPrefetchStatus(), PrefetchStatus::kPrefetchNotUsedCookiesChanged);
  SetPrefetchStatus(PrefetchStatus::kPrefetchNotUsedCookiesChanged);
  UpdateServingPageMetrics();
  CancelStreamingURLLoaderIfNotServing();
}

void PrefetchContainer::OnPrefetchStarted() {
  SetLoadState(PrefetchContainer::LoadState::kStarted);
  time_prefetch_started_ = base::TimeTicks::Now();
}

bool PrefetchContainer::HasSameReferringURLForMetrics(
    const PrefetchContainer& other) const {
  return referring_url_hash_.has_value() &&
         other.referring_url_hash_.has_value() &&
         referring_url_hash_ == other.referring_url_hash_;
}

GURL PrefetchContainer::GetCurrentURL() const {
  return GetCurrentSinglePrefetchToPrefetch().url_;
}

GURL PrefetchContainer::GetPreviousURL() const {
  return GetPreviousSinglePrefetchToPrefetch().url_;
}

bool PrefetchContainer::IsRendererInitiated() const {
  return prefetch_type_.IsRendererInitiated();
}

bool PrefetchContainer::IsIsolatedNetworkContextRequiredForCurrentPrefetch()
    const {
  const SinglePrefetch& this_prefetch = GetCurrentSinglePrefetchToPrefetch();
  return this_prefetch.is_isolated_network_context_required_;
}

bool PrefetchContainer::IsIsolatedNetworkContextRequiredForPreviousRedirectHop()
    const {
  const SinglePrefetch& previous_prefetch =
      GetPreviousSinglePrefetchToPrefetch();
  return previous_prefetch.is_isolated_network_context_required_;
}

base::WeakPtr<PrefetchResponseReader>
PrefetchContainer::GetResponseReaderForCurrentPrefetch() {
  const SinglePrefetch& this_prefetch = GetCurrentSinglePrefetchToPrefetch();
  CHECK(this_prefetch.response_reader_);
  return this_prefetch.response_reader_->GetWeakPtr();
}

bool PrefetchContainer::Reader::IsIsolatedNetworkContextRequiredToServe()
    const {
  const SinglePrefetch& this_prefetch = GetCurrentSinglePrefetchToServe();
  return this_prefetch.is_isolated_network_context_required_;
}

base::WeakPtr<PrefetchResponseReader>
PrefetchContainer::Reader::GetCurrentResponseReaderToServeForTesting() {
  return GetCurrentSinglePrefetchToServe().response_reader_->GetWeakPtr();
}

PrefetchContainer::ServableState PrefetchContainer::Reader::GetServableState(
    base::TimeDelta cacheable_duration) const {
  return GetPrefetchContainer()->GetServableState(cacheable_duration);
}
bool PrefetchContainer::Reader::HasPrefetchStatus() const {
  return GetPrefetchContainer()->HasPrefetchStatus();
}
PrefetchStatus PrefetchContainer::Reader::GetPrefetchStatus() const {
  return GetPrefetchContainer()->GetPrefetchStatus();
}

bool PrefetchContainer::IsProxyRequiredForURL(const GURL& url) const {
  return IsCrossOriginRequest(url::Origin::Create(url)) &&
         prefetch_type_.IsProxyRequiredWhenCrossOrigin();
}

void PrefetchContainer::MakeResourceRequest(
    const net::HttpRequestHeaders& additional_headers) {
  // |AddRedirectHop| updates this request later on. Anything here that should
  // be changed on redirect should happen there.

  const GURL& url = GetURL();
  url::Origin origin = url::Origin::Create(url);
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, origin, origin,
      net::SiteForCookies::FromOrigin(origin));

  auto priority = [&] {
    if (IsSpeculationRuleType(prefetch_type_.trigger_type())) {
      // This may seem inverted (surely eager prefetches would be higher
      // priority), but the fact that we're doing this at all for more
      // conservative candidates suggests a strong engagement signal.
      //
      // TODO(crbug.com/40276985): Ideally, we would actually use a combination
      // of the actual engagement seen (rather than the minimum required to
      // trigger the candidate) and the declared eagerness, and update them as
      // the prefetch becomes increasingly likely.
      blink::mojom::SpeculationEagerness eagerness =
          prefetch_type_.GetEagerness();
      switch (eagerness) {
        case blink::mojom::SpeculationEagerness::kConservative:
          return net::RequestPriority::MEDIUM;
        case blink::mojom::SpeculationEagerness::kModerate:
          return net::RequestPriority::LOW;
        case blink::mojom::SpeculationEagerness::kEager:
          return net::RequestPriority::IDLE;
      }
    } else {
      if (base::FeatureList::IsEnabled(
              features::kPrefetchNetworkPriorityForEmbedders)) {
        return net::RequestPriority::MEDIUM;
      } else {
        return net::RequestPriority::IDLE;
      }
    }
  }();

  mojo::PendingRemote<network::mojom::DevToolsObserver>
      devtools_observer_remote;
  if (std::optional<mojo::PendingRemote<network::mojom::DevToolsObserver>>
          devtools_observer = MakeSelfOwnedNetworkServiceDevToolsObserver()) {
    devtools_observer_remote = std::move(devtools_observer.value());
  }

  // If we ever implement prefetching for subframes, this value should be
  // reconsidered, as this causes us to reset the site for cookies on cross-site
  // redirect.
  const bool is_main_frame = true;

  auto request = CreateResourceRequestForNavigation(
      net::HttpRequestHeaders::kGetMethod, url,
      network::mojom::RequestDestination::kDocument, referrer_, isolation_info,
      std::move(devtools_observer_remote), priority, is_main_frame);

  // Note: Even without LOAD_DISABLE_CACHE, a cross-site prefetch uses a
  // separate network context, which means responses cached before the prefetch
  // are not visible to the prefetch, and anything cached by this request will
  // not be visible outside of the network context.
  request->load_flags = net::LOAD_PREFETCH;

  request->headers.MergeFrom(additional_headers_);
  request->headers.MergeFrom(additional_headers);
  request->headers.SetHeader(blink::kPurposeHeaderName,
                             blink::kSecPurposePrefetchHeaderValue);
  request->headers.SetHeader(blink::kSecPurposeHeaderName,
                             GetSecPurposeHeaderValue(url));
  request->headers.SetHeader("Upgrade-Insecure-Requests", "1");

  // Sec-Speculation-Tags is set only when the prefetch is triggered
  // by speculation rules and it is not cross-site prefetch.
  // To see more details:
  // https://github.com/WICG/nav-speculation/blob/main/speculation-rules-tags.md#the-cross-site-case
  if (speculation_rules_tags_.has_value() && !IsCrossSiteRequest(origin)) {
    CHECK(IsSpeculationRuleType(prefetch_type_.trigger_type()));
    std::optional<std::string> serialized_list =
        speculation_rules_tags_->ConvertStringToHeaderString();
    CHECK(serialized_list.has_value());
    request->headers.SetHeader(blink::kSecSpeculationTagsHeaderName,
                               serialized_list.value());
  }

  // There are sometimes other headers that are set during navigation.  These
  // aren't yet supported for prefetch, including browsing topics.

  request->devtools_request_id = RequestId();

  AddClientHintsHeaders(origin, &request->headers);
  if (should_append_variations_header_) {
    AddXClientDataHeader(*request.get());
  }

  // `URLLoaderNetworkServiceObserver`
  // (`request->trusted_params->url_loader_network_observer`) is NOT set here,
  // because for prefetching request we don't want to ask users e.g. for
  // authentication/cert errors, and instead make the prefetch fail. Because of
  // this, `ServiceWorkerClient::GetOngoingNavigationRequestBeforeCommit()` is
  // never called. `NavPrefetchBrowserTest` has the corresponding test coverage.

  resource_request_ = std::move(request);
}

void PrefetchContainer::UpdateReferrer(
    const GURL& new_referrer_url,
    const network::mojom::ReferrerPolicy& new_referrer_policy) {
  referrer_.url = new_referrer_url;
  referrer_.policy = new_referrer_policy;
}

void PrefetchContainer::AddClientHintsHeaders(
    const url::Origin& origin,
    net::HttpRequestHeaders* request_headers) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchClientHints)) {
    return;
  }
  BrowserContext* browser_context = browser_context_.get();
  if (!browser_context_) {
    return;
  }
  ClientHintsControllerDelegate* client_hints_delegate =
      browser_context->GetClientHintsControllerDelegate();
  if (!client_hints_delegate) {
    return;
  }

  // TODO(crbug.com/41497015): Consider supporting UA override mode here
  const bool is_ua_override_on = false;
  net::HttpRequestHeaders client_hints_headers;
  if (is_javascript_enabled_) {
    // Historically, `AddClientHintsHeadersToPrefetchNavigation` added
    // Client Hints headers iff `is_javascript_enabled_`, so the `if` block here
    // is to persist the behavior.
    // TODO(crbug.com/394716357): Revisit if we really want to allow prefetch
    // for non-Javascript enabled profile/origins.
    AddClientHintsHeadersToPrefetchNavigation(
        origin, &client_hints_headers, browser_context, client_hints_delegate,
        is_ua_override_on);
  }

  // Merge in the client hints which are suitable to include given this is a
  // prefetch, and potentially a cross-site only. (This logic might need to be
  // revisited if we ever supported prefetching in another site's partition,
  // such as in a subframe.)
  const bool is_cross_site = IsCrossSiteRequest(origin);
  const auto cross_site_behavior =
      features::kPrefetchClientHintsCrossSiteBehavior.Get();
  if (!is_cross_site ||
      cross_site_behavior ==
          features::PrefetchClientHintsCrossSiteBehavior::kAll) {
    request_headers->MergeFrom(client_hints_headers);
  } else if (cross_site_behavior ==
             features::PrefetchClientHintsCrossSiteBehavior::kLowEntropy) {
    for (const auto& [ch, header] : network::GetClientHintToNameMap()) {
      if (blink::IsClientHintSentByDefault(ch)) {
        std::optional<std::string> header_value =
            client_hints_headers.GetHeader(header);
        if (header_value) {
          request_headers->SetHeader(header, std::move(header_value).value());
        }
      }
    }
  }
}

std::ostream& operator<<(std::ostream& ostream,
                         const PrefetchContainer& prefetch_container) {
  return ostream << "PrefetchContainer[" << &prefetch_container
                 << ", Key=" << prefetch_container.key() << "]";
}

std::ostream& operator<<(std::ostream& ostream,
                         const PrefetchContainer::Key& prefetch_key) {
  ostream << "(";
  if (const auto* token = std::get_if<std::optional<blink::DocumentToken>>(
          &prefetch_key.referring_document_token_or_nik_)) {
    token->has_value() ? ostream << token->value()
                       : ostream << "(empty document token)";
  } else {
    ostream << std::get<net::NetworkIsolationKey>(
                   prefetch_key.referring_document_token_or_nik_)
                   .ToDebugString();
  }
  ostream << ", " << prefetch_key.url() << ")";
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         PrefetchContainer::LoadState state) {
  switch (state) {
    case PrefetchContainer::LoadState::kNotStarted:
      return ostream << "NotStarted";
    case PrefetchContainer::LoadState::kEligible:
      return ostream << "Eligible";
    case PrefetchContainer::LoadState::kFailedIneligible:
      return ostream << "FailedIneligible";
    case PrefetchContainer::LoadState::kStarted:
      return ostream << "Started";
    case PrefetchContainer::LoadState::kFailedHeldback:
      return ostream << "FailedHeldback";
  }
}

CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& ostream,
    PrefetchContainer::ServableState servable_state) {
  switch (servable_state) {
    case PrefetchContainer::ServableState::kNotServable:
      return ostream << "NotServable";
    case PrefetchContainer::ServableState::kServable:
      return ostream << "Servable";
    case PrefetchContainer::ServableState::kShouldBlockUntilHeadReceived:
      return ostream << "ShouldBlockUntilHeadReceived";
    case PrefetchContainer::ServableState::kShouldBlockUntilEligibilityGot:
      return ostream << "ShouldBlockUntilEligibilityGot";
  }
}

PrefetchContainer::SinglePrefetch::SinglePrefetch(
    const GURL& url,
    bool is_isolated_network_context_required,
    bool is_reusable)
    : url_(url),
      is_isolated_network_context_required_(
          is_isolated_network_context_required),
      response_reader_(
          base::MakeRefCounted<PrefetchResponseReader>(is_reusable)) {}

PrefetchContainer::SinglePrefetch::~SinglePrefetch() {
  CHECK(response_reader_);
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(response_reader_));
}

const char* PrefetchContainer::GetSecPurposeHeaderValue(
    const GURL& request_url) const {
  switch (preload_pipeline_info_->planned_max_preloading_type()) {
    case PreloadingType::kPrefetch:
      if (IsProxyRequiredForURL(request_url)) {
        return blink::kSecPurposePrefetchAnonymousClientIpHeaderValue;
      } else {
        return blink::kSecPurposePrefetchHeaderValue;
      }
    case PreloadingType::kPrerender:
      if (IsProxyRequiredForURL(request_url)) {
        // Note that this path would be reachable if a prefetch ahead of
        // prerender were triggered with a speculation candidate with
        // `requires_anonymous_client_ip_when_cross_origin`. But such
        // Speculation Rules are discarded in blink.
        //
        // See
        // https://github.com/WICG/nav-speculation/blob/main/triggers.md#requirements
        NOTREACHED();
      } else {
        return blink::kSecPurposePrefetchPrerenderHeaderValue;
      }
    case PreloadingType::kUnspecified:
    case PreloadingType::kPreconnect:
    case PreloadingType::kNoStatePrefetch:
    case PreloadingType::kLinkPreview:
      NOTREACHED();
  }
}

void PrefetchContainer::OnInitialPrefetchFailedIneligible(
    PreloadingEligibility eligibility) {
  CHECK(redirect_chain_.size() == 1);
  CHECK_NE(eligibility, PreloadingEligibility::kEligible);
  if (request_status_listener_) {
    request_status_listener_->OnPrefetchStartFailedGeneric();
  }
}

void PrefetchContainer::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PrefetchContainer::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PrefetchContainer::IsExactMatch(const GURL& url) const {
  return url == GetURL();
}

bool PrefetchContainer::IsNoVarySearchHeaderMatch(const GURL& url) const {
  const std::optional<net::HttpNoVarySearchData>& no_vary_search_data =
      GetNoVarySearchData();
  return no_vary_search_data &&
         no_vary_search_data->AreEquivalent(url, GetURL());
}

bool PrefetchContainer::ShouldWaitForNoVarySearchHeader(const GURL& url) const {
  const std::optional<net::HttpNoVarySearchData>& no_vary_search_hint =
      GetNoVarySearchHint();
  return !GetNonRedirectHead() && no_vary_search_hint &&
         no_vary_search_hint->AreEquivalent(url, GetURL());
}

void PrefetchContainer::OnUnregisterCandidate(
    const GURL& navigated_url,
    bool is_served,
    std::optional<base::TimeDelta> blocked_duration) {
  // Note that this method can be called with `is_in_dtor_` true.
  //
  // TODO(crbug.com/356314759): Avoid calling this with `is_in_dtor_`
  // true.

  if (is_served) {
    navigated_to_ = true;

    UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.AfterClick.RedirectChainSize",
                             redirect_chain_.size());
  }

  RecordPrefetchMatchingBlockedNavigationHistogram(
      blocked_duration.has_value());

  RecordBlockUntilHeadDurationHistogram(blocked_duration, is_served);

  // Note that `PreloadingAttemptImpl::SetIsAccurateTriggering()` is called for
  // prefetch in
  //
  // - A. `PreloadingDataImpl::DidStartNavigation()`
  // - B. Here
  //
  // A covers prefetches that satisfy `bool(GetNonRedirectHead())` at that
  // timing. B covers almost all ones that were once potentially matching to the
  // navigation, including that was `kBlockUntilHead` state.
  //
  // Note that multiple calls are safe and set a correct value.
  //
  // Historical note: Before No-Vary-Search hint, the decision to use a
  // prefetched response was made at A. With No-Vary-Search hint the decision to
  // use an in-flight prefetched response is delayed until the headers are
  // received from the server. This happens after `DidStartNavigation()`. At
  // this point in the code we have already decided we are going to use the
  // prefetch, so we can safely call `SetIsAccurateTriggering()`.
  if (auto attempt = preloading_attempt()) {
    static_cast<PreloadingAttemptImpl*>(attempt.get())
        ->SetIsAccurateTriggering(navigated_url);
  }
}

void PrefetchContainer::MigrateNewlyAdded(
    std::unique_ptr<PrefetchContainer> added) {
  // `inherited_preload_pipeline_infos_` increases only if it is managed under
  // `PrefetchService`.
  CHECK(added->inherited_preload_pipeline_infos_.empty());

  // Propagate eligibility (and status) to `added`.
  //
  // Assume we don't. (*) case is problematic.
  //
  // - If eligibility is not got, eligibility and status will be propagated by
  //   the following `OnEligibilityCheckComplete()` and
  //   `SetPrefetchStatusWithoutUpdatingTriggeringOutcome()`.
  // - If eligibility is got and ineligible, this `PrefetchContainer` is
  //   `kNotServed` and `MigrateNewlyAdded()` is not called.
  // - If eligibility is got and `kEligible`:
  //   - If status is not got, status will be propagated by the following
  //     `SetPrefetchStatusWithoutUpdatingTriggeringOutcome()`.
  //     - If status is eventually `kPrefetchSuccessful` or
  //       `kPrefetchResponseUsed`, `kPrefetchResponseUsed` will be propagated
  //       at the prefetch matching end.
  //     - If status is eventually failure, status is propagated, but
  //       eligibility is `kUnspecified`. (*)
  //   - If status is got and `kPrefetchSuccessful` or `kPrefetchResponseUsed`,
  //     `kPrefetchResponseUsed` will be propagated at the prefetch matching
  //     end.
  //   - If status is got and failure, this `PrefetchContainer` is `kNotServed`
  //     and `MigrateNewlyAdded()` is not called.
  //
  // In (*), `PrerenderHost` have to cancel prerender with eligibility
  // `kUnspecified` and status failure. It's relatively complicated condition.
  // See a test
  // `PrerendererImplBrowserTestPrefetchAhead.PrefetchMigratedPrefetchFailurePrerenderFailure`.
  //
  // To make things simple, we propagate both eligibility and status.
  added->preload_pipeline_info_->SetPrefetchEligibility(
      preload_pipeline_info_->prefetch_eligibility());
  if (preload_pipeline_info_->prefetch_status().has_value()) {
    added->preload_pipeline_info_->SetPrefetchStatus(
        preload_pipeline_info_->prefetch_status().value());
  }

  inherited_preload_pipeline_infos_.push_back(
      std::move(added->preload_pipeline_info_));
  is_likely_ahead_of_prerender_ |= added->is_likely_ahead_of_prerender_;
}

void PrefetchContainer::NotifyPrefetchRequestWillBeSent(
    const network::mojom::URLResponseHeadPtr* redirect_head) {
  if (IsDecoy()) {
    return;
  }

  auto* rfh = RenderFrameHostImpl::FromID(referring_render_frame_host_id_);
  auto* ftn = FrameTreeNode::From(rfh);
  // Don't emit CDP events if the trigger is not Spec Rules or the document
  // isn't alive.
  if (!rfh) {
    return;
  }
  CHECK(ftn);

  if (redirect_head && *redirect_head) {
    const network::mojom::URLResponseHeadDevToolsInfoPtr info =
        network::ExtractDevToolsInfo(**redirect_head);
    const GURL url = GetPreviousURL();
    std::pair<const GURL&, const network::mojom::URLResponseHeadDevToolsInfo&>
        redirect_info{url, *info.get()};
    devtools_instrumentation::OnPrefetchRequestWillBeSent(
        *ftn, RequestId(), rfh->GetLastCommittedURL(), *GetResourceRequest(),
        std::move(redirect_info));
  } else {
    devtools_instrumentation::OnPrefetchRequestWillBeSent(
        *ftn, RequestId(), rfh->GetLastCommittedURL(), *GetResourceRequest(),
        std::nullopt);
  }
}

void PrefetchContainer::NotifyPrefetchResponseReceived(
    const network::mojom::URLResponseHead& head) {
  // Ensured by the caller `PrefetchService::OnPrefetchResponseStarted()`.
  CHECK(!IsDecoy());

  time_url_request_started_ = head.load_timing.request_start;

  // DevTools plumbing.
  auto* ftn = FrameTreeNode::From(
      RenderFrameHostImpl::FromID(referring_render_frame_host_id_));
  // Don't emit CDP events if the trigger is not Spec Rules or the document
  // isn't alive.
  if (!ftn) {
    return;
  }
  devtools_instrumentation::OnPrefetchResponseReceived(ftn, RequestId(),
                                                       GetCurrentURL(), head);
}

void PrefetchContainer::NotifyPrefetchRequestComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  // Ensured by the caller `PrefetchService::OnPrefetchResponseStarted()`.
  CHECK(!IsDecoy());

  auto* ftn = FrameTreeNode::From(
      RenderFrameHostImpl::FromID(referring_render_frame_host_id_));
  // Don't emit CDP events if the trigger is not Spec Rules or the document
  // isn't alive.
  if (!ftn) {
    return;
  }

  devtools_instrumentation::OnPrefetchRequestComplete(ftn, RequestId(),
                                                      completion_status);
}

std::optional<mojo::PendingRemote<network::mojom::DevToolsObserver>>
PrefetchContainer::MakeSelfOwnedNetworkServiceDevToolsObserver() {
  if (IsDecoy()) {
    return std::nullopt;
  }

  auto* ftn = FrameTreeNode::From(
      RenderFrameHostImpl::FromID(referring_render_frame_host_id_));
  // Return nullopt if the trigger is not Spec Rules or the document isn't
  // alive.
  if (!ftn) {
    return std::nullopt;
  }

  return NetworkServiceDevToolsObserver::MakeSelfOwned(ftn);
}

void PrefetchContainer::MaybeRecordPrefetchStatusToUMA(
    PrefetchStatus prefetch_status) {
  if (prefetch_status_recorded_to_uma_) {
    return;
  }

  base::UmaHistogramEnumeration("Preloading.Prefetch.PrefetchStatus",
                                prefetch_status);
  prefetch_status_recorded_to_uma_ = true;
}

void PrefetchContainer::OnServiceWorkerStateDetermined(
    PrefetchServiceWorkerState service_worker_state) {
  switch (service_worker_state_) {
    case PrefetchServiceWorkerState::kDisallowed:
      CHECK_EQ(service_worker_state, PrefetchServiceWorkerState::kDisallowed);
      break;
    case PrefetchServiceWorkerState::kAllowed:
      CHECK_NE(service_worker_state, PrefetchServiceWorkerState::kAllowed);
      service_worker_state_ = service_worker_state;
      break;
    case PrefetchServiceWorkerState::kControlled:
      NOTREACHED();
  }
}

void PrefetchContainer::RecordDurationFromAdded() {
  if (!time_added_to_prefetch_service_.has_value()) {
    return;
  }

  if (!time_initial_eligibility_got_.has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToInitialEligibility.",
          GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type_,
                                                  embedder_histogram_suffix_),
      }),
      time_initial_eligibility_got_.value() -
          time_added_to_prefetch_service_.value());

  if (!time_prefetch_started_.has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToPrefetchStarted.",
          GetMetricsSuffixTriggerTypeAndEagerness(prefetch_type_,
                                                  embedder_histogram_suffix_),
      }),
      time_prefetch_started_.value() - time_added_to_prefetch_service_.value());

  if (!time_url_request_started_.has_value()) {
    return;
  }

  base::UmaHistogramTimes(base::StrCat({
                              "Prefetch.PrefetchContainer."
                              "AddedToURLRequestStarted.",
                              GetMetricsSuffixTriggerTypeAndEagerness(
                                  prefetch_type_, embedder_histogram_suffix_),
                          }),
                          time_url_request_started_.value() -
                              time_added_to_prefetch_service_.value());

  if (!time_header_determined_successfully_.has_value()) {
    return;
  }

  base::UmaHistogramTimes(base::StrCat({
                              "Prefetch.PrefetchContainer."
                              "AddedToHeaderDeterminedSuccessfully.",
                              GetMetricsSuffixTriggerTypeAndEagerness(
                                  prefetch_type_, embedder_histogram_suffix_),
                          }),
                          time_header_determined_successfully_.value() -
                              time_added_to_prefetch_service_.value());

  if (!time_prefetch_completed_successfully_.has_value()) {
    return;
  }

  base::UmaHistogramTimes(base::StrCat({
                              "Prefetch.PrefetchContainer."
                              "AddedToPrefetchCompletedSuccessfully.",
                              GetMetricsSuffixTriggerTypeAndEagerness(
                                  prefetch_type_, embedder_histogram_suffix_),
                          }),
                          time_prefetch_completed_successfully_.value() -
                              time_added_to_prefetch_service_.value());
}

void PrefetchContainer::RecordPrefetchMatchingBlockedNavigationHistogram(
    bool blocked_until_head) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.",
           GetMetricsSuffixTriggerTypeAndEagerness(
               prefetch_type_, embedder_histogram_suffix_)}),
      blocked_until_head);
}

void PrefetchContainer::RecordBlockUntilHeadDurationHistogram(
    const std::optional<base::TimeDelta>& blocked_duration,
    bool served) {
  base::UmaHistogramTimes(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.",
                    served ? "Served." : "NotServed.",
                    GetMetricsSuffixTriggerTypeAndEagerness(
                        prefetch_type_, embedder_histogram_suffix_)}),
      blocked_duration.value_or(base::Seconds(0)));
}
}  // namespace content
