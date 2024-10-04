// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/browser/devtools/devtools_instrumentation.h"
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
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/client_hints.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/prefetch_browser_callbacks.h"
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
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
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

    case PreloadingEligibility::kEligible:
    default:
      // Other ineligible cases are not used in `PrefetchService`.
      NOTREACHED_IN_MIGRATION();
      return PrefetchStatus::kPrefetchIneligiblePreloadingDisabled;
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
      return PreloadingTriggeringOutcome::kFailure;
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchAllowed:
    case PrefetchStatus::kPrefetchNotStarted:
      return std::nullopt;
  }
  return std::nullopt;
}

// Returns true if SetPrefetchStatus(|status|) can be called after a prefetch
// has already been marked as failed. We ignore such status updates as they
// may end up overwriting the initial failure reason.
bool StatusUpdateIsPossibleAfterFailure(PrefetchStatus status) {
  switch (status) {
    case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
    case PrefetchStatus::kPrefetchIsStale:
      return true;
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
    case PrefetchStatus::kPrefetchAllowed:
    case PrefetchStatus::kPrefetchNotStarted:
    case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
    case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
      return false;
  }
}

// Please follow go/preloading-dashboard-updates if a new outcome enum or a
// failure reason enum is added.
void SetTriggeringOutcomeAndFailureReasonFromStatus(
    PreloadingAttempt* attempt,
    const GURL& url,
    std::optional<PrefetchStatus> old_prefetch_status,
    PrefetchStatus new_prefetch_status) {
  if (old_prefetch_status &&
      old_prefetch_status.value() == PrefetchStatus::kPrefetchResponseUsed) {
    // Skip this update if the triggering outcome has already been updated
    // to kSuccess.
    return;
  }

  if (old_prefetch_status &&
      TriggeringOutcomeFromStatus(old_prefetch_status.value()) ==
          PreloadingTriggeringOutcome::kFailure) {
    CHECK(StatusUpdateIsPossibleAfterFailure(new_prefetch_status))
        << "old_prefetch_status: "
        << static_cast<int>(old_prefetch_status.value())
        << " -> new_prefetch_status: " << static_cast<int>(new_prefetch_status);
    CHECK(TriggeringOutcomeFromStatus(new_prefetch_status) ==
          PreloadingTriggeringOutcome::kFailure);
    // Skip this update if the triggering outcome has already been updated to
    // kFailure.
      return;
  }

  if (attempt) {
    switch (new_prefetch_status) {
      case PrefetchStatus::kPrefetchNotFinishedInTime:
        attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kRunning);
        break;
      case PrefetchStatus::kPrefetchSuccessful:
        // A successful prefetch means the response is ready to be used for the
        // next navigation.
        attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kReady);
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
          attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kReady);
        }
        attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kSuccess);
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
        attempt->SetFailureReason(
            ToPreloadingFailureReason(new_prefetch_status));
        break;
      case PrefetchStatus::kPrefetchHeldback:
      // `kPrefetchAllowed` will soon transition into `kPrefetchNotStarted`.
      case PrefetchStatus::kPrefetchAllowed:
      case PrefetchStatus::kPrefetchNotStarted:
        // `kPrefetchNotStarted` is set in
        // `PrefetchService::OnGotEligibilityResult` when the container is
        // pushed onto the prefetch queue, which occurs before the holdback
        // status is determined in `PrefetchService::StartSinglePrefetch`.
        // After the container is queued and before it is sent for prefetch, the
        // only status change is when the container is popped from the queue but
        // heldback. This is covered by attempt's holdback status. For these two
        // reasons this PrefetchStatus does not fire a `SetTriggeringOutcome`.
        break;
      case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
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

void RecordWasBlockedUntilHeadWhenServingHistogram(
    const PrefetchType& prefetch_type,
    bool blocked_until_head) {
  CHECK(!UseNewWaitLoop());

  if (IsSpeculationRuleType(prefetch_type.trigger_type())) {
    base::UmaHistogramBoolean(
        base::StringPrintf(
            "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
            GetPrefetchEagernessHistogramSuffix(prefetch_type.GetEagerness())
                .c_str()),
        blocked_until_head);
  } else {
    // TODO(crbug.com/40946257, crbug.com/40898833): Extend the metrics for
    // embedder triggers.
  }
}

void RecordPrefetchMatchingBlockedNavigationWithPrefetchHistogram(
    const PrefetchType& prefetch_type,
    bool blocked_until_head) {
  CHECK(UseNewWaitLoop());

  if (IsSpeculationRuleType(prefetch_type.trigger_type())) {
    base::UmaHistogramBoolean(
        base::StringPrintf(
            "PrefetchProxy.AfterClick."
            "PrefetchMatchingBlockedNavigationWithPrefetch.%s",
            GetPrefetchEagernessHistogramSuffix(prefetch_type.GetEagerness())
                .c_str()),
        blocked_until_head);
  } else {
    // TODO(crbug.com/40946257, crbug.com/40898833): Extend the metrics for
    // embedder triggers.
  }
}

void RecordBlockUntilHeadDurationHistogram(
    const PrefetchType& prefetch_type,
    const base::TimeDelta& block_until_head_duration,
    bool served) {
  CHECK(!UseNewWaitLoop());

  if (IsSpeculationRuleType(prefetch_type.trigger_type())) {
    base::UmaHistogramTimes(
        base::StringPrintf(
            "PrefetchProxy.AfterClick.BlockUntilHeadDuration.%s.%s",
            served ? "Served" : "NotServed",
            GetPrefetchEagernessHistogramSuffix(prefetch_type.GetEagerness())
                .c_str()),
        block_until_head_duration);
  } else {
    // TODO(crbug.com/40946257, crbug.com/40898833): Extend the metrics for
    // embedder triggers.
  }
}

void RecordBlockUntilHeadDuration2Histogram(
    const PrefetchType& prefetch_type,
    const base::TimeDelta block_until_head_duration,
    bool served) {
  CHECK(UseNewWaitLoop());

  if (IsSpeculationRuleType(prefetch_type.trigger_type())) {
    base::UmaHistogramTimes(
        base::StringPrintf(
            "PrefetchProxy.AfterClick.BlockUntilHeadDuration2.%s.%s",
            served ? "Served" : "NotServed",
            GetPrefetchEagernessHistogramSuffix(prefetch_type.GetEagerness())
                .c_str()),
        block_until_head_duration);
  } else {
    // TODO(crbug.com/40946257, crbug.com/40898833): Extend the metrics for
    // embedder triggers.
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

// TODO(crbug.com/353490734): Inline it. We made it a method due to
// this rule:
// https://chromium.googlesource.com/chromium/src/+/master/tools/metrics/histograms/README.md#don_t-use-same-inline-string-in-multiple-places
// If callsite is only one, we can inline it again.
void RecordAfterClickRedirectChainSize(size_t redirect_chain_size) {
  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.AfterClick.RedirectChainSize",
                           redirect_chain_size);
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
  explicit SinglePrefetch(const GURL& url, const url::Origin& referring_origin);
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
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager,
    base::WeakPtr<PreloadingAttempt> attempt)
    : PrefetchContainer(
          referring_render_frame_host.GetGlobalId(),
          referring_render_frame_host.GetLastCommittedOrigin(),
          base::FastHash(
              referring_render_frame_host.GetLastCommittedURL().spec()),
          PrefetchContainer::Key(referring_document_token, url),
          prefetch_type,
          referrer,
          std::move(no_vary_search_hint),
          prefetch_document_manager,
          referring_render_frame_host.GetBrowserContext()->GetWeakPtr(),
          GetUkmSourceId(referring_render_frame_host),
          std::move(attempt),
          /*holdback_status_override=*/std::nullopt,
          referring_render_frame_host.GetDevToolsNavigationToken(),
          /* prefetch_start_callback=*/std::nullopt,
          WebContentsImpl::FromRenderFrameHostImpl(&referring_render_frame_host)
              ->GetOrCreateWebPreferences()
              .javascript_enabled) {
  CHECK(prefetch_type_.IsRendererInitiated());
}

PrefetchContainer::PrefetchContainer(
    WebContents& referring_web_contents,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    const std::optional<url::Origin>& referring_origin,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    base::WeakPtr<PreloadingAttempt> attempt,
    std::optional<PreloadingHoldbackStatus> holdback_status_override)
    : PrefetchContainer(
          GlobalRenderFrameHostId(),
          referring_origin.value_or(url::Origin()),
          /*referring_url_hash=*/std::nullopt,
          PrefetchContainer::Key(
              std::optional<blink::DocumentToken>(std::nullopt),
              url),
          prefetch_type,
          referrer,
          std::move(no_vary_search_hint),
          /*prefetch_document_manager=*/nullptr,
          referring_web_contents.GetBrowserContext()->GetWeakPtr(),
          ukm::kInvalidSourceId,
          std::move(attempt),
          holdback_status_override,
          /*initiator_devtools_navigation_token=*/std::nullopt,
          /* prefetch_start_callback=*/std::nullopt,
          referring_web_contents.GetOrCreateWebPreferences()
              .javascript_enabled) {
  CHECK(!prefetch_type_.IsRendererInitiated());
  CHECK(PrefetchBrowserInitiatedTriggersEnabled());
}

PrefetchContainer::PrefetchContainer(
    BrowserContext* browser_context,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    bool javascript_enabled,
    const std::optional<url::Origin>& referring_origin,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    base::WeakPtr<PreloadingAttempt> attempt,
    std::optional<PrefetchStartCallback> prefetch_start_callback)
    : PrefetchContainer(GlobalRenderFrameHostId(),
                        referring_origin.value_or(url::Origin()),
                        /*referring_url_hash=*/std::nullopt,
                        PrefetchContainer::Key(
                            std::optional<blink::DocumentToken>(std::nullopt),
                            url),
                        prefetch_type,
                        referrer,
                        std::move(no_vary_search_hint),
                        /*prefetch_document_manager=*/nullptr,
                        browser_context->GetWeakPtr(),
                        ukm::kInvalidSourceId,
                        std::move(attempt),
                        /*holdback_status_override=*/std::nullopt,
                        /*initiator_devtools_navigation_token=*/std::nullopt,
                        std::move(prefetch_start_callback),
                        javascript_enabled) {
  CHECK(!prefetch_type_.IsRendererInitiated());
  CHECK(PrefetchBrowserInitiatedTriggersEnabled());
}

PrefetchContainer::PrefetchContainer(
    const GlobalRenderFrameHostId& referring_render_frame_host_id,
    const url::Origin& referring_origin,
    const std::optional<size_t>& referring_url_hash,
    const PrefetchContainer::Key& key,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager,
    base::WeakPtr<BrowserContext> browser_context,
    ukm::SourceId ukm_source_id,
    base::WeakPtr<PreloadingAttempt> attempt,
    std::optional<PreloadingHoldbackStatus> holdback_status_override,
    std::optional<base::UnguessableToken> initiator_devtools_navigation_token,
    std::optional<PrefetchStartCallback> prefetch_start_callback,
    bool is_javascript_enabled)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      referring_origin_(referring_origin),
      referring_url_hash_(referring_url_hash),
      key_(key),
      prefetch_type_(prefetch_type),
      referrer_(referrer),
      no_vary_search_hint_(std::move(no_vary_search_hint)),
      prefetch_document_manager_(std::move(prefetch_document_manager)),
      browser_context_(std::move(browser_context)),
      ukm_source_id_(ukm_source_id),
      request_id_(base::UnguessableToken::Create().ToString()),
      attempt_(std::move(attempt)),
      holdback_status_override_(holdback_status_override),
      initiator_devtools_navigation_token_(
          std::move(initiator_devtools_navigation_token)),
      prefetch_start_callback_(std::move(prefetch_start_callback)),
      is_javascript_enabled_(is_javascript_enabled) {
  redirect_chain_.push_back(
      std::make_unique<SinglePrefetch>(GetURL(), referring_origin_));
}

PrefetchContainer::~PrefetchContainer() {
  is_in_dtor_ = true;

  // Ideally, this method should be called just before dtor.
  // https://chromium-review.googlesource.com/c/chromium/src/+/5657659/comments/0cfb14c0_3050963e
  //
  // TODO(crbug.com/356314759): Do it.
  if (UseNewWaitLoop()) {
    OnWillBeDestroyed();
  }

  CancelStreamingURLLoaderIfNotServing();

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

  if (!UseNewWaitLoop()) {
    UnblockPrefetchMatchResolver();
  }
}

void PrefetchContainer::OnWillBeDestroyed() {
  CHECK(UseNewWaitLoop());

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

void PrefetchContainer::SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
    PrefetchStatus prefetch_status) {
  prefetch_status_ = prefetch_status;

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
          preloading_trigger_outcome.value(), prefetch_status, RequestId());
    }
  }
}

void PrefetchContainer::SetPrefetchStatus(PrefetchStatus prefetch_status) {
  SetTriggeringOutcomeAndFailureReasonFromStatus(
      attempt_.get(), GetURL(),
      /*old_prefetch_status=*/prefetch_status_,
      /*new_prefetch_status=*/prefetch_status);
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
      NOTREACHED_IN_MIGRATION();
      break;

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

void PrefetchContainer::OnEligibilityCheckComplete(
    PreloadingEligibility eligibility) {
  SinglePrefetch& this_prefetch = GetCurrentSinglePrefetchToPrefetch();
  this_prefetch.eligibility_ = eligibility;
  bool is_eligible = eligibility == PreloadingEligibility::kEligible;

  if (redirect_chain_.size() == 1) {
    // This case is for just the URL that was originally requested to be
    // prefetched.
    if (is_eligible) {
      SetLoadState(LoadState::kEligible);
    } else {
      SetLoadState(LoadState::kFailedIneligible);
      SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
          PrefetchStatusFromIneligibleReason(eligibility));
      OnInitialPrefetchFailedIneligible(eligibility);
    }

    if (attempt_) {
      // Please follow go/preloading-dashboard-updates if a new eligibility is
      // added.
      attempt_->SetEligibility(eligibility);
    }

    // Recording an eligiblity for PrefetchReferringPageMetrics.
    // TODO(crbug.com/40946257): Current code doesn't support
    // PrefetchReferringPageMetrics when the prefetch is initiated by browser.
    if (IsRendererInitiated()) {
      if (prefetch_document_manager_) {
        prefetch_document_manager_->OnEligibilityCheckComplete(is_eligible);
      }
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
  updated_headers.SetHeader("Sec-Purpose",
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
      redirect_info.new_url, referring_origin_));
}

void PrefetchContainer::MarkCrossSiteContaminated() {
  is_cross_site_contaminated_ = true;
}

void PrefetchContainer::AddXClientDataHeader(
    network::ResourceRequest& request) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchXClientDataHeader)) {
    return;
  }

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

void PrefetchContainer::StopAllCookieListeners() {
  for (const auto& single_prefetch : redirect_chain_) {
    if (single_prefetch->cookie_listener_) {
      single_prefetch->cookie_listener_->StopListening();
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

  // We don't want any of the cookie listeners for this prefetch to pick up
  // changes from the copy.
  prefetch_container_->StopAllCookieListeners();

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

const base::WeakPtr<PrefetchStreamingURLLoader>&
PrefetchContainer::GetStreamingURLLoader() const {
  // Streaming loaders scheduled for deletion shouldn't be used.
  CHECK(!streaming_loader_ ||
        !streaming_loader_->IsDeletionScheduledForCHECK());
  return streaming_loader_;
}

bool PrefetchContainer::IsStreamingURLLoaderDeletionScheduledForTesting()
    const {
  return streaming_loader_ && streaming_loader_->IsDeletionScheduledForCHECK();
}

const PrefetchResponseReader* PrefetchContainer::GetNonRedirectResponseReader()
    const {
  if (redirect_chain_.empty()) {
    return nullptr;
  }
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

PrefetchRequestHandler PrefetchContainer::Reader::CreateRequestHandler() {
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

void PrefetchContainer::StartBlockUntilHead(
    base::OnceCallback<void(PrefetchContainer&)>
        on_maybe_determined_head_callback,
    base::TimeDelta timeout) {
  CHECK(!UseNewWaitLoop());

  on_maybe_determined_head_callback_ =
      std::move(on_maybe_determined_head_callback);

  if (timeout.is_positive()) {
    // TODO(crbug.com/40274818): See the comment on
    // `OnGetPrefetchToServe()`.
    block_until_head_timer_ = std::make_unique<base::OneShotTimer>();
    block_until_head_timer_->Start(
        FROM_HERE, timeout,
        base::BindOnce(&PrefetchContainer::UnblockPrefetchMatchResolver,
                       GetWeakPtr()));
  }
}

void PrefetchContainer::OnDeterminedHead() {
  CHECK(!UseNewWaitLoop());

  // Propagates the header to `no_vary_search_data_` if a non-redirect response
  // header is got.
  //
  // TODO(crbug.com/40946257): Current code doesn't support NVS for
  // browser-initated triggers.
  if (IsRendererInitiated()) {
    auto* rfhi_can_be_null =
        RenderFrameHostImpl::FromID(referring_render_frame_host_id_);
    MaybeSetNoVarySearchData(rfhi_can_be_null);
  }

  UnblockPrefetchMatchResolver();
}

void PrefetchContainer::OnDeterminedHead2() {
  CHECK(UseNewWaitLoop());

  // Propagates the header to `no_vary_search_data_` if a non-redirect response
  // header is got.
  //
  // TODO(crbug.com/40946257): Current code doesn't support NVS for
  // browser-initated triggers.
  if (IsRendererInitiated()) {
    auto* rfhi_can_be_null =
        RenderFrameHostImpl::FromID(referring_render_frame_host_id_);
    MaybeSetNoVarySearchData(rfhi_can_be_null);
  }

  for (auto& observer : observers_) {
    observer.OnDeterminedHead(*this);
  }
}

void PrefetchContainer::MaybeSetNoVarySearchData(RenderFrameHost* rfh) {
  CHECK(!no_vary_search_data_.has_value());

  if (!GetNonRedirectHead()) {
    return;
  }

  no_vary_search_data_ =
      no_vary_search::ProcessHead(*GetNonRedirectHead(), GetURL(), rfh);
}

void PrefetchContainer::UnblockPrefetchMatchResolver() {
  CHECK(!UseNewWaitLoop());

  block_until_head_timer_.reset();

  if (on_maybe_determined_head_callback_) {
    std::move(on_maybe_determined_head_callback_).Run(*this);
  }
}

void PrefetchContainer::StartTimeoutTimer(
    base::TimeDelta timeout,
    base::OnceClosure on_timeout_callback) {
  CHECK(!timeout_timer_);
  timeout_timer_ = std::make_unique<base::OneShotTimer>();
  timeout_timer_->Start(FROM_HERE, timeout, std::move(on_timeout_callback));
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
  // SpeculationHostDevToolsObserver::OnPrefetchBodyDataReceived with body of
  // the response.
  const auto& devtools_observer = GetDevToolsObserver();
  if (devtools_observer) {
    devtools_observer->OnPrefetchRequestComplete(RequestId(),
                                                 completion_status);
  }

  int net_error = completion_status.error_code;
  int64_t body_length = completion_status.decoded_body_length;

  RecordPrefetchProxyPrefetchMainframeNetError(net_error);

  // Updates the prefetch's status if it hasn't been updated since the request
  // first started. For the prefetch to reach the network stack, it must have
  // `PrefetchStatus::kPrefetchAllowed` or beyond.
  DCHECK(HasPrefetchStatus());
  if (GetPrefetchStatus() == PrefetchStatus::kPrefetchNotFinishedInTime) {
    SetPrefetchStatus(net_error == net::OK
                          ? PrefetchStatus::kPrefetchSuccessful
                          : PrefetchStatus::kPrefetchFailedNetError);
    UpdateServingPageMetrics();
  }

  if (net_error == net::OK) {
    RecordPrefetchProxyPrefetchMainframeBodyLength(body_length);
  }

  if (GetPrefetchStatus() == PrefetchStatus::kPrefetchSuccessful) {
    // TODO(crbug.com/40946257): Current code doesn't support
    // PrefetchReferringPageMetrics when the prefetch is initiated by browser.
    if (IsRendererInitiated()) {
      if (prefetch_document_manager_) {
        prefetch_document_manager_->OnPrefetchSuccessful(this);
      }
    }
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
           << "(streaming_loader=" << streaming_loader_.get() << ")"
           << "(redirect_chain.empty=" << redirect_chain_.empty() << ")";
  // Can only block until head if the request has been started using a
  // streaming URL loader and head/failure/redirect hasn't been received yet.
  if (streaming_loader_ && !redirect_chain_.empty() &&
      redirect_chain_.back()->response_reader_->IsWaitingForResponse()) {
    return ServableState::kShouldBlockUntilHeadReceived;
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

void PrefetchContainer::SimulateAttemptAtRequestStartForTest() {
  if (attempt_) {
    attempt_->SetEligibility(PreloadingEligibility::kEligible);
    attempt_->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
  }
  SetPrefetchStatus(PrefetchStatus::kPrefetchAllowed);
  SetPrefetchStatus(PrefetchStatus::kPrefetchNotFinishedInTime);
}

void PrefetchContainer::SimulateAttemptAtInterceptorForTest() {
  if (attempt_) {
    attempt_->SetEligibility(PreloadingEligibility::kEligible);
    attempt_->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
  }
  SetPrefetchStatus(PrefetchStatus::kPrefetchAllowed);
  SetPrefetchStatus(PrefetchStatus::kPrefetchSuccessful);
}

void PrefetchContainer::OnDetectedCookiesChange() {
  CHECK_NE(GetPrefetchStatus(), PrefetchStatus::kPrefetchNotUsedCookiesChanged);
  SetPrefetchStatus(PrefetchStatus::kPrefetchNotUsedCookiesChanged);
  on_detected_cookies_change_called_ = true;
  UpdateServingPageMetrics();
  CancelStreamingURLLoaderIfNotServing();
}

void PrefetchContainer::OnDetectedCookiesChange2() {
  CHECK(UseNewWaitLoop());

  // If `kPrefetchNewWaitLoop` is enabled, multiple `PrefetchMatchResolver2` can
  // wait the same `PrefetchContainer`. So, `OnDetectedCookiesChange2()` can be
  // called multiple times, unlike `OnDetectedCookiesChange()`.
  //
  // TODO(crbug.com/353490734): Remove this comment and merge
  // `OnDetectedCookiesChange()` to it when removing `kPrefetchNewWaitLoop` as
  // this comment is just a note about the difference to the old path.
  //
  // Do not call `OnDetectedCookiesChange()` multiple times even if
  // `OnDetectedCookiesChange2()` is called multiple times.
  if (on_detected_cookies_change_called_) {
    return;
  }

  OnDetectedCookiesChange();
}

void PrefetchContainer::OnPrefetchStarted() {
  SetLoadState(PrefetchContainer::LoadState::kStarted);
  if (prefetch_start_callback_.has_value()) {
    CHECK(prefetch_start_callback_.value());
    std::move(prefetch_start_callback_.value())
        .Run(PrefetchStartResultCode::kSuccess);
  }
}

// TODO(crbug.com/40274818): We might be waiting on PrefetchContainer's head
// from multiple navigations.
// E.g. We might wait from one navigation but not use the prefetch, and
// then we can use the prefetch in a separate navigation without waiting
// for the head. We need to keep track of blocked_until_head_start_time_ per
// each navigation for this PrefetchContainer.
void PrefetchContainer::OnGetPrefetchToServe(bool blocked_until_head) {
  CHECK(!UseNewWaitLoop());

  // OnGetPrefetchToServe is called before we start waiting for head, and
  // when the prefetch is used from `prefetches_ready_to_serve_`.
  // If the prefetch had to wait for head, `blocked_until_head_start_time_`
  // will already be set. Only record in the histogram when the
  // `blocked_until_head_start_time_` is not set yet.
  if (!blocked_until_head_start_time_) {
    RecordWasBlockedUntilHeadWhenServingHistogram(prefetch_type_,
                                                  blocked_until_head);
  }
  if (blocked_until_head) {
    blocked_until_head_start_time_ = base::TimeTicks::Now();
  }
}

void PrefetchContainer::OnReturnPrefetchToServe(bool served,
                                                const GURL& navigated_url) {
  CHECK(!UseNewWaitLoop());

  if (served) {
    RecordAfterClickRedirectChainSize(redirect_chain_.size());
    navigated_to_ = true;
  }

  if (blocked_until_head_start_time_.has_value()) {
    RecordBlockUntilHeadDurationHistogram(
        prefetch_type_,
        base::TimeTicks::Now() - blocked_until_head_start_time_.value(),
        served);
  }

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
  return !referring_origin_.IsSameOriginWith(url) &&
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
  network::ResourceRequest::TrustedParams trusted_params;
  trusted_params.isolation_info = isolation_info;

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = "GET";
  request->referrer = referrer_.url;
  request->referrer_policy =
      Referrer::ReferrerPolicyForUrlRequest(referrer_.policy);
  request->enable_load_timing = true;
  // Note: Even without LOAD_DISABLE_CACHE, a cross-site prefetch uses a
  // separate network context, which means responses cached before the prefetch
  // are not visible to the prefetch, and anything cached by this request will
  // not be visible outside of the network context.
  request->load_flags = net::LOAD_PREFETCH;
  request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  request->headers.MergeFrom(additional_headers);
  request->headers.SetHeader(kCorsExemptPurposeHeaderName, "prefetch");
  request->headers.SetHeader("Sec-Purpose", GetSecPurposeHeaderValue(url));
  request->headers.SetHeader("Upgrade-Insecure-Requests", "1");

  // Remove the user agent header if it was set so that the network context's
  // default is used.
  request->headers.RemoveHeader("User-Agent");

  // There are sometimes other headers that are set during navigation.  These
  // aren't yet supported for prefetch, including browsing topics.

  request->trusted_params = trusted_params;
  request->site_for_cookies = trusted_params.isolation_info.site_for_cookies();

  // This causes us to reset the site for cookies on cross-site redirect. This
  // is correct as long as we are looking at top-level navigations. If we ever
  // implement prefetching for subframes, this will need to consider that.
  // See also the code which sets this in |NavigationUrlLoaderImpl|.
  request->update_first_party_url_on_redirect = true;

  request->devtools_request_id = RequestId();

  request->priority = [&] {
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
      // TODO(crbug.com/40946257): Revisit and update after each embedder
      // trigger is introduced, as the appropriate value may differ based on its
      // property and triggering condition. For now, it is set to IDLE as a safe
      // default value.
      return net::RequestPriority::IDLE;
    }
  }();

  AddClientHintsHeaders(origin, &request->headers);
  AddXClientDataHeader(*request.get());

  const auto& devtools_observer = GetDevToolsObserver();
  if (devtools_observer && !IsDecoy()) {
    request->trusted_params->devtools_observer =
        devtools_observer->MakeSelfOwnedNetworkServiceDevToolsObserver();
  }

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
  AddClientHintsHeadersToPrefetchNavigation(
      origin, &client_hints_headers, browser_context, client_hints_delegate,
      is_ua_override_on, is_javascript_enabled_);

  // Merge in the client hints which are suitable to include given this is a
  // prefetch, and potentially a cross-site only. (This logic might need to be
  // revisited if we ever supported prefetching in another site's partition,
  // such as in a subframe.)
  const bool is_same_site =
      net::SchemefulSite(referring_origin_) == net::SchemefulSite(origin);
  const auto cross_site_behavior =
      features::kPrefetchClientHintsCrossSiteBehavior.Get();
  if (is_same_site ||
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
  if (const auto* token = absl::get_if<std::optional<blink::DocumentToken>>(
          &prefetch_key.referring_document_token_or_nik_)) {
    token->has_value() ? ostream << token->value()
                       : ostream << "(empty document token)";
  } else {
    ostream << absl::get<net::NetworkIsolationKey>(
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
  }
}

PrefetchContainer::SinglePrefetch::SinglePrefetch(
    const GURL& url,
    const url::Origin& referring_origin)
    : url_(url),
      is_isolated_network_context_required_(
          net::SchemefulSite(referring_origin) != net::SchemefulSite(url_)),
      response_reader_(base::MakeRefCounted<PrefetchResponseReader>()) {}

PrefetchContainer::SinglePrefetch::~SinglePrefetch() {
  CHECK(response_reader_);
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(response_reader_));
}

const char* PrefetchContainer::GetSecPurposeHeaderValue(
    const GURL& request_url) const {
  auto* attempt = static_cast<PreloadingAttemptImpl*>(attempt_.get());
  if (attempt) {
    switch (attempt->planned_max_preloading_type()) {
      case PreloadingType::kPrefetch:
        if (IsProxyRequiredForURL(request_url)) {
          return "prefetch;anonymous-client-ip";
        } else {
          return "prefetch";
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
          return "prefetch;prerender";
        }
      case PreloadingType::kUnspecified:
      case PreloadingType::kPreconnect:
      case PreloadingType::kNoStatePrefetch:
      case PreloadingType::kLinkPreview:
        NOTREACHED();
    }
  } else {
    // Note that the `PreloadingAttempt` is null means that the initiating
    // document has gone, which also implies there is no prerender that can use
    // the result of this prefetch.

    if (IsProxyRequiredForURL(request_url)) {
      return "prefetch;anonymous-client-ip";
    } else {
      return "prefetch";
    }
  }
}

void PrefetchContainer::OnInitialPrefetchFailedIneligible(
    PreloadingEligibility eligibility) {
  CHECK(redirect_chain_.size() == 1);
  CHECK_NE(eligibility, PreloadingEligibility::kEligible);
  if (prefetch_start_callback_.has_value()) {
    CHECK(prefetch_start_callback_.value());
    std::move(prefetch_start_callback_.value())
        .Run(GetPrefetchFailedIneligibleStartResultCode(eligibility));
  }
}

PrefetchStartResultCode
PrefetchContainer::GetPrefetchFailedIneligibleStartResultCode(
    PreloadingEligibility eligibility) {
  CHECK_NE(eligibility, PreloadingEligibility::kEligible);
  return PrefetchStartResultCode::kFailed;
}

void PrefetchContainer::AddObserver(Observer* observer) {
  CHECK(UseNewWaitLoop());

  observers_.AddObserver(observer);
}

void PrefetchContainer::RemoveObserver(Observer* observer) {
  CHECK(UseNewWaitLoop());

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

  CHECK(UseNewWaitLoop());

  if (is_served) {
    navigated_to_ = true;
    RecordAfterClickRedirectChainSize(redirect_chain_.size());
  }

  RecordPrefetchMatchingBlockedNavigationWithPrefetchHistogram(
      prefetch_type_, blocked_duration.has_value());

  if (blocked_duration.has_value()) {
    RecordBlockUntilHeadDuration2Histogram(prefetch_type_,
                                           blocked_duration.value(), is_served);
  }

  // See the comment in `PrefetchContainer::OnReturnPrefetchToServe()`.
  if (auto attempt = preloading_attempt()) {
    static_cast<PreloadingAttemptImpl*>(attempt.get())
        ->SetIsAccurateTriggering(navigated_url);
  }
}

}  // namespace content
