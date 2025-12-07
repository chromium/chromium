// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/devtools/network_service_devtools_observer.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/preloading/prefetch/prefetch_servable_state.h"
#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_single_redirect_hop.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/preloading_trigger_type_impl.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/proxy_lookup_client_impl.h"
#include "content/browser/preloading/speculation_rules/speculation_rules_tags.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/client_hints.h"
#include "content/public/browser/frame_accept_header.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/prefetch_request_status_listener.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_features.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_request_info.h"
#include "net/url_request/redirect_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "url/gurl.h"

namespace content {
namespace {

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
    case PreloadingType::kPrerenderUntilScript:
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

void AddAwAdditionalHeaders(net::HttpRequestHeaders& request_headers,
                            const net::HttpRequestHeaders& additional_headers) {
  // Ignore "User-Agent" override by `additional_headers` if UA override fix are
  // enabled.
  // TODO(crbug.com/383779480): Add tests.
  if (base::FeatureList::IsEnabled(
          features::kPreloadingRespectUserAgentOverride)) {
    net::HttpRequestHeaders additional_headers_without_ua = additional_headers;
    additional_headers_without_ua.RemoveHeader(
        net::HttpRequestHeaders::kUserAgent);
    request_headers.MergeFrom(additional_headers_without_ua);
  } else {
    request_headers.MergeFrom(additional_headers);
  }
}

}  // namespace

// static
std::unique_ptr<PrefetchContainer> PrefetchContainer::Create(
    base::PassKey<PrefetchService>,
    std::unique_ptr<const PrefetchRequest> request) {
  return std::make_unique<PrefetchContainer>(base::PassKey<PrefetchContainer>(),
                                             std::move(request));
}

// static
std::unique_ptr<PrefetchContainer> PrefetchContainer::CreateForTesting(
    std::unique_ptr<const PrefetchRequest> request) {
  return std::make_unique<PrefetchContainer>(base::PassKey<PrefetchContainer>(),
                                             std::move(request));
}

PrefetchContainer::PrefetchContainer(
    base::PassKey<PrefetchContainer>,
    std::unique_ptr<const PrefetchRequest> request)
    : request_(std::move(request)),
      referrer_(request_->initial_referrer()),
      request_id_(base::UnguessableToken::Create().ToString()) {
  CHECK(request_);

  is_likely_ahead_of_prerender_ =
      CalculateIsLikelyAheadOfPrerender(request_->preload_pipeline_info());

  redirect_chain_.push_back(std::make_unique<PrefetchSingleRedirectHop>(
      *this, GetURL(), IsCrossSiteRequest(url::Origin::Create(GetURL()))));

  // Disallow prefetching ServiceWorker-controlled responses for isolated
  // network contexts.
  if (!features::IsPrefetchServiceWorkerEnabled(request_->browser_context()) ||
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
  RecordPrefetchDurationHistogram();
  RecordPrefetchMatchMissedToPrefetchStartedHistogram();
  RecordPrefetchContainerServedCountHistogram();

  ukm::SourceId ukm_source_id = ukm::kInvalidSourceId;
  if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
    ukm_source_id = renderer_initiator_info->ukm_source_id();
  }
  ukm::builders::PrefetchProxy_PrefetchedResource builder(ukm_source_id);
  builder.SetResourceType(/*mainframe*/ 1);
  builder.SetStatus(static_cast<int>(
      prefetch_status_.value_or(PrefetchStatus::kPrefetchNotStarted)));
  builder.SetLinkClicked(served_count_ > 0);

  if (GetNonRedirectResponseReader()) {
    GetNonRedirectResponseReader()->RecordOnPrefetchContainerDestroyed(
        base::PassKey<PrefetchContainer>(), builder);
  }

  if (probe_result_) {
    builder.SetISPFilteringStatus(static_cast<int>(probe_result_.value()));
  }

  // TODO(crbug.com/40215782): Get the navigation start time and set the
  // NavigationStartToFetchStartMs field of the PrefetchProxy.PrefetchedResource
  // UKM event.

  builder.Record(ukm::UkmRecorder::Get());

  if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
    if (renderer_initiator_info->prefetch_document_manager()) {
      renderer_initiator_info->prefetch_document_manager()
          ->PrefetchWillBeDestroyed(this);
    }
  }
}

void PrefetchContainer::OnWillBeDestroyed() {
  for (auto& observer : observers_) {
    observer.OnWillBeDestroyed(*this);
  }
}

PrefetchServingHandle PrefetchContainer::CreateServingHandle() {
  return PrefetchServingHandle(GetWeakPtr(), 0);
}

const std::vector<std::unique_ptr<PrefetchSingleRedirectHop>>&
PrefetchContainer::redirect_chain(base::PassKey<PrefetchServingHandle>) const {
  return redirect_chain_;
}

void PrefetchContainer::SetProbeResult(base::PassKey<PrefetchServingHandle>,
                                       PrefetchProbeResult probe_result) {
  probe_result_ = probe_result;
}

std::optional<PreloadingTriggeringOutcome>
PrefetchContainer::TriggeringOutcomeFromStatusForServingHandle(
    base::PassKey<PrefetchServingHandle>,
    PrefetchStatus prefetch_status) {
  return TriggeringOutcomeFromStatus(prefetch_status);
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
      NOTREACHED() << "PrefetchStatus illegal transition: "
                      "(old_prefetch_status, new_prefetch_status) = ("
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

  if (request().attempt()) {
    switch (new_prefetch_status) {
      case PrefetchStatus::kPrefetchNotFinishedInTime:
        request().attempt()->SetTriggeringOutcome(
            PreloadingTriggeringOutcome::kRunning);
        break;
      case PrefetchStatus::kPrefetchSuccessful:
        // A successful prefetch means the response is ready to be used for the
        // next navigation.
        request().attempt()->SetTriggeringOutcome(
            PreloadingTriggeringOutcome::kReady);
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
          request().attempt()->SetTriggeringOutcome(
              PreloadingTriggeringOutcome::kReady);
        }
        request().attempt()->SetTriggeringOutcome(
            PreloadingTriggeringOutcome::kSuccess);
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
        request().attempt()->SetFailureReason(
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
  request().preload_pipeline_info().SetPrefetchStatus(prefetch_status);
  for (auto& preload_pipeline_info : inherited_preload_pipeline_infos_) {
    preload_pipeline_info->SetPrefetchStatus(prefetch_status);
  }

  // Currently DevTools only supports when the prefetch is initiated by
  // renderer.
  if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
    std::optional<PreloadingTriggeringOutcome> preloading_trigger_outcome =
        TriggeringOutcomeFromStatus(prefetch_status);
    if (renderer_initiator_info->devtools_navigation_token().has_value() &&
        preloading_trigger_outcome.has_value()) {
      devtools_instrumentation::DidUpdatePrefetchStatus(
          FrameTreeNode::From(renderer_initiator_info->GetRenderFrameHost()),
          renderer_initiator_info->devtools_navigation_token().value(),
          GetURL(), request().preload_pipeline_info().id(),
          preloading_trigger_outcome.value(), prefetch_status, RequestId());
    }
  }
}

void PrefetchContainer::SetPrefetchMatchMissedTimeForMetrics(
    base::TimeTicks time) {
  CHECK(!time_prefetch_match_missed_);
  time_prefetch_match_missed_ = time;
}

void PrefetchContainer::SetPrefetchStatus(PrefetchStatus prefetch_status) {
  // The concept of `PreloadingAttempt`'s `PreloadingTriggeringOutcome` is to
  // record the outcomes of started triggers. Therefore, this should
  // only be called once prefetching has actually started, and not for
  // ineligible or eligibled but not started triggers (e.g., holdback triggers,
  // triggers waiting on a queue).
  switch (GetLoadState()) {
    case LoadState::kStarted:
    case LoadState::kDeterminedHead:
    case LoadState::kFailedDeterminedHead:
    case LoadState::kCompleted:
    case LoadState::kFailed:
      SetTriggeringOutcomeAndFailureReasonFromStatus(prefetch_status);
      break;
    case LoadState::kNotStarted:
    case LoadState::kEligible:
    case LoadState::kFailedIneligible:
    case LoadState::kFailedHeldback:
      break;
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

  PrefetchNetworkContext* network_context =
      GetNetworkContext(is_isolated_network_context_required);
  if (network_context) {
    return network_context;
  }

  GlobalRenderFrameHostId referring_render_frame_host_id;
  if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
    referring_render_frame_host_id =
        renderer_initiator_info->GetRenderFrameHostId();
  }

  auto owned_network_context = std::make_unique<PrefetchNetworkContext>(
      is_isolated_network_context_required, request().prefetch_type(),
      referring_render_frame_host_id, request().referring_origin());
  network_context = owned_network_context.get();
  network_contexts_.emplace(is_isolated_network_context_required,
                            std::move(owned_network_context));

  return network_context;
}

PrefetchNetworkContext* PrefetchContainer::GetNetworkContext(
    bool is_isolated_network_context_required) const {
  const auto network_context_itr =
      network_contexts_.find(is_isolated_network_context_required);
  if (network_context_itr == network_contexts_.end()) {
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

void PrefetchContainer::SetLoadState(LoadState new_load_state) {
  if (base::FeatureList::IsEnabled(features::kPrefetchGracefulNotification)) {
    CHECK(!is_in_dtor_);
  }

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

    case LoadState::kDeterminedHead:
    case LoadState::kFailedDeterminedHead:
      CHECK_EQ(load_state_, LoadState::kStarted);
      break;

    case LoadState::kCompleted:
      // `kFailedDeterminedHead` never transitions to successful `kCompleted`.
      CHECK_EQ(load_state_, LoadState::kDeterminedHead);
      break;

    case LoadState::kFailed:
      // Failures can happen after successful `kDeterminedHead`.
      CHECK(load_state_ == LoadState::kDeterminedHead ||
            load_state_ == LoadState::kFailedDeterminedHead);
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
  prefetch_container_metrics_.time_added_to_prefetch_service =
      base::TimeTicks::Now();
}

void PrefetchContainer::OnEligibilityCheckComplete(
    PreloadingEligibility eligibility) {
  request().preload_pipeline_info().SetPrefetchEligibility(eligibility);
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

    if (request().attempt()) {
      // Please follow go/preloading-dashboard-updates if a new eligibility is
      // added.
      request().attempt()->SetEligibility(eligibility);
    }

    prefetch_container_metrics_.time_initial_eligibility_got =
        base::TimeTicks::Now();

    // Recording an eligiblity for PrefetchReferringPageMetrics.
    // TODO(crbug.com/40946257): Current code doesn't support
    // PrefetchReferringPageMetrics when the prefetch is initiated by browser.
    if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
      if (renderer_initiator_info->prefetch_document_manager()) {
        renderer_initiator_info->prefetch_document_manager()
            ->OnEligibilityCheckComplete(is_eligible);
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

void PrefetchContainer::AddRedirectHop(const net::RedirectInfo& redirect_info) {
  CHECK(resource_request_);

  // There are sometimes other headers that are modified during navigation
  // redirects; see |NavigationRequest::OnRedirectChecksComplete| (including
  // some which are added by throttles). These aren't yet supported for
  // prefetch, including browsing topics and client hints.
  // TODO(crbug.com/441612842): Support User-Agent overrides.
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
  if (request().speculation_rules_tags().has_value() &&
      !IsCrossSiteRequest(url::Origin::Create(redirect_info.new_url))) {
    std::optional<std::string> serialized_list =
        request().speculation_rules_tags()->ConvertStringToHeaderString();
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

  redirect_chain_.push_back(std::make_unique<PrefetchSingleRedirectHop>(
      *this, redirect_info.new_url,
      IsCrossSiteRequest(url::Origin::Create(redirect_info.new_url))));
}

bool PrefetchContainer::IsCrossSiteRequest(const url::Origin& origin) const {
  return request().referring_origin().has_value() &&
         !net::SchemefulSite::IsSameSite(request().referring_origin().value(),
                                         origin);
}

bool PrefetchContainer::IsCrossOriginRequest(const url::Origin& origin) const {
  return request().referring_origin().has_value() &&
         !request().referring_origin().value().IsSameOriginWith(origin);
}

void PrefetchContainer::MarkCrossSiteContaminated() {
  is_cross_site_contaminated_ = true;
}

void PrefetchContainer::AddXClientDataHeader(
    network::ResourceRequest& resource_request) {
  if (request().browser_context()) {
    // Add X-Client-Data header with experiment IDs from field trials.
    variations::AppendVariationsHeader(
        resource_request.url,
        request().browser_context()->IsOffTheRecord()
            ? variations::InIncognito::kYes
            : variations::InIncognito::kNo,
        variations::SignedIn::kNo, &resource_request);
  }
}

void PrefetchContainer::RegisterCookieListener(
    network::mojom::CookieManager* cookie_manager) {
  PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToPrefetch();
  this_prefetch.cookie_listener_ = PrefetchCookieListener::MakeAndRegister(
      this_prefetch.url_, cookie_manager);
}

void PrefetchContainer::PauseAllCookieListeners() {
  // TODO(crbug.com/377440445): Consider whether we actually need to
  // pause/resume all single prefetch's cookie listener during each single
  // prefetch's isolated cookie copy.
  for (const auto& single_redirect_hop : redirect_chain_) {
    if (single_redirect_hop->cookie_listener_) {
      single_redirect_hop->cookie_listener_->PauseListening();
    }
  }
}

void PrefetchContainer::ResumeAllCookieListeners() {
  // TODO(crbug.com/377440445): Consider whether we actually need to
  // pause/resume all single prefetch's cookie listener during each single
  // prefetch's isolated cookie copy.
  for (const auto& single_redirect_hop : redirect_chain_) {
    if (single_redirect_hop->cookie_listener_) {
      single_redirect_hop->cookie_listener_->ResumeListening();
    }
  }
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

void PrefetchContainer::CancelStreamingURLLoaderIfNotServing() {
  if (!streaming_loader_) {
    return;
  }
  streaming_loader_->CancelIfNotServing();
  streaming_loader_.reset();
}

void PrefetchContainer::OnDeterminedHead(bool is_successful_determined_head) {
  if (base::FeatureList::IsEnabled(features::kPrefetchGracefulNotification) &&
      is_in_dtor_) {
    // This can be called due to the loader cancellation during the
    // `PrefetchContainer` destruction. No state changes should be made and
    // observers shouldn't be notified during destruction.
    return;
  }

  SetLoadState(is_successful_determined_head
                   ? LoadState::kDeterminedHead
                   : LoadState::kFailedDeterminedHead);

  if (GetNonRedirectHead()) {
    prefetch_container_metrics_.time_header_determined_successfully =
        base::TimeTicks::Now();
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
  RenderFrameHostImpl* rfhi_can_be_null = nullptr;
  if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
    rfhi_can_be_null = renderer_initiator_info->GetRenderFrameHost();
  }
  no_vary_search_data_ = no_vary_search::ProcessHead(
      *GetNonRedirectHead(), GetURL(), rfhi_can_be_null);
}

void PrefetchContainer::StartTimeoutTimerIfNeeded(
    base::OnceClosure on_timeout_callback) {
  if (request().ttl().is_positive()) {
    CHECK(!timeout_timer_);
    timeout_timer_ = std::make_unique<base::OneShotTimer>();
    timeout_timer_->Start(FROM_HERE, request().ttl(),
                          std::move(on_timeout_callback));
  }
}

// static
void PrefetchContainer::SetPrefetchResponseCompletedCallbackForTesting(
    PrefetchResponseCompletedCallbackForTesting callback) {
  GetPrefetchResponseCompletedCallbackForTesting() =  // IN-TEST
      std::move(callback);
}

void PrefetchContainer::OnPrefetchCompleteInternal(
    const network::URLLoaderCompletionStatus& completion_status) {
  DVLOG(1) << *this << "::OnPrefetchComplete";

  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.Prefetch.RedirectChainSize",
                           redirect_chain_.size());

  if (GetNonRedirectResponseReader()) {
    UpdatePrefetchRequestMetrics(
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
    prefetch_container_metrics_.time_prefetch_completed_successfully =
        base::TimeTicks::Now();
    RecordPrefetchProxyPrefetchMainframeBodyLength(body_length);
  }

  const PrefetchStatus prefetch_status = GetPrefetchStatus();

  if (prefetch_status == PrefetchStatus::kPrefetchSuccessful) {
    // TODO(crbug.com/40946257): Current code doesn't support
    // PrefetchReferringPageMetrics when the prefetch is initiated by browser.
    if (auto* renderer_initiator_info = request().GetRendererInitiatorInfo()) {
      if (renderer_initiator_info->prefetch_document_manager()) {
        renderer_initiator_info->prefetch_document_manager()
            ->OnPrefetchSuccessful(this);
      }
    }
  }

  if (auto* browser_initiator_info = request().GetBrowserInitiatorInfo()) {
    if (auto* listener = browser_initiator_info->request_status_listener()) {
      switch (prefetch_status) {
        case PrefetchStatus::kPrefetchSuccessful:
        case PrefetchStatus::kPrefetchResponseUsed:
          listener->OnPrefetchResponseCompleted();
          break;
        case PrefetchStatus::kPrefetchFailedNon2XX: {
          int response_code =
              GetNonRedirectHead()
                  ? GetNonRedirectHead()->headers->response_code()
                  : 0;
          listener->OnPrefetchResponseServerError(response_code);
          break;
        }
        default:
          listener->OnPrefetchResponseError();
          break;
      }
    }
  }
}

// TODO(https://crbug.com/432518638): We should be able to calculate
// `is_success` and `completion_status` from the last `PrefetchResponseReader`.
// Before https://crbug.com/432518638 is fixed, we explicitly plumb them here to
// ensure the correct `PrefetchResponseReader`'s states are used.
void PrefetchContainer::OnPrefetchComplete(
    bool is_success,
    const network::URLLoaderCompletionStatus& completion_status) {
  SetLoadState(is_success ? LoadState::kCompleted : LoadState::kFailed);
  OnPrefetchCompleteInternal(completion_status);

  std::optional<int> response_code = std::nullopt;
  int net_error = completion_status.error_code;
  if (net_error == net::OK && GetNonRedirectHead() &&
      GetNonRedirectHead()->headers) {
    response_code = GetNonRedirectHead()->headers->response_code();
  }
  for (auto& observer : observers_) {
    observer.OnPrefetchCompletedOrFailed(*this, completion_status,
                                         response_code);
  }

  if (GetPrefetchResponseCompletedCallbackForTesting()) {
    GetPrefetchResponseCompletedCallbackForTesting().Run(  // IN-TEST
        GetWeakPtr());
  }
}

void PrefetchContainer::UpdatePrefetchRequestMetrics(
    const network::mojom::URLResponseHead* head) {
  DVLOG(1) << *this << "::UpdatePrefetchRequestMetrics:"
           << "head = " << head;

  if (head)
    header_latency_ =
        head->load_timing.receive_headers_end - head->load_timing.request_start;
}

PrefetchServableState PrefetchContainer::GetServableState(
    base::TimeDelta cacheable_duration) const {
  // We allow the differences between `GetServableStateInternal()` and
  // `match_resolver_action.ToServableState()` because we know the latter should
  // be the correct behavior.
  auto is_known_allowed_exception =
      [&](PrefetchServableState servable_state,
          const PrefetchMatchResolverAction& match_resolver_action) {
        // `GetCodeOfPrefetchServableStateAndPrefetchMatchResolverActionForDebug()
        // == 2181`
        // Failed test: PrefetchServiceTest.IneligibleRedirectCookies/*
        //
        // `OnDeterminedHead()` is called when redirect is judged as ineligible,
        // with `GetNonRedirectResponseReader()` null. Ideally, we should treat
        // this case as `PrefetchServableState::kNotServable`, but the current
        // `GetServableStateInternal()` returns
        // `PrefetchServableState::kShouldBlockUntilHeadReceived`. We will keep
        // the current behavior and fix it by replacing the implementation with
        // `GetMatchResolverAction()`.
        //
        // TODO(crbug.com/455448933): Do it.
        if (servable_state ==
                PrefetchServableState::kShouldBlockUntilHeadReceived &&
            match_resolver_action.kind() ==
                PrefetchMatchResolverAction::ActionKind::kDrop &&
            match_resolver_action.prefetch_container_load_state() ==
                PrefetchContainer::LoadState::kFailedDeterminedHead) {
          return true;
        }

        return false;
      };

  PrefetchServableState servable_state =
      GetServableStateInternal(cacheable_duration);
  PrefetchMatchResolverAction match_resolver_action =
      GetMatchResolverAction(cacheable_duration);

  if (servable_state != match_resolver_action.ToServableState() &&
      !is_known_allowed_exception(servable_state, match_resolver_action)) {
    // We are going to switch from the old implementation
    // (`GetServableStateInternal()`) to the new one
    // (`match_resolver_action.ToServableState()`), and check the behavior
    // difference, if any.
    SCOPED_CRASH_KEY_NUMBER(
        "PrefetchContainer", "GSS_ssma",
        GetCodeOfPrefetchServableStateAndPrefetchMatchResolverActionForDebug(
            servable_state, match_resolver_action));
    DUMP_WILL_BE_NOTREACHED();
  }

  return servable_state;
}

PrefetchServableState PrefetchContainer::GetServableStateInternal(
    base::TimeDelta cacheable_duration) const {
  // Servable if the non-redirect response (either fully or partially
  // received body) is servable.
  if (GetNonRedirectResponseReader() &&
      GetNonRedirectResponseReader()->Servable(cacheable_duration)) {
    return PrefetchServableState::kServable;
  }

  DVLOG(1) << *this << "(GetServableState)"
           << "(streaming_loader=" << GetStreamingURLLoader().get()
           << ", LoadState=" << load_state_ << ")";
  // Can only block until head if the request has been started using a
  // streaming URL loader and head/failure/redirect hasn't been received yet.
  if (GetStreamingURLLoader() &&
      redirect_chain_.back()->response_reader_->IsWaitingForResponse()) {
    return PrefetchServableState::kShouldBlockUntilHeadReceived;
  }

  if (features::UsePrefetchPrerenderIntegration()) {
    switch (load_state_) {
      case LoadState::kNotStarted:
      case LoadState::kEligible:
        return PrefetchServableState::kShouldBlockUntilEligibilityGot;
      case LoadState::kFailedIneligible:
      case LoadState::kStarted:
      case LoadState::kDeterminedHead:
      case LoadState::kFailedDeterminedHead:
      case LoadState::kCompleted:
      case LoadState::kFailed:
      case LoadState::kFailedHeldback:
        // nop
        break;
    }
  }

  return PrefetchServableState::kNotServable;
}

PrefetchMatchResolverAction PrefetchContainer::GetMatchResolverAction(
    base::TimeDelta cacheable_duration) const {
  switch (load_state_) {
    case LoadState::kNotStarted:
      if (features::UsePrefetchPrerenderIntegration()) {
        return PrefetchMatchResolverAction(
            PrefetchMatchResolverAction::ActionKind::kWait, load_state_,
            std::nullopt);
      } else {
        return PrefetchMatchResolverAction(
            PrefetchMatchResolverAction::ActionKind::kDrop, load_state_,
            std::nullopt);
      }
    case LoadState::kEligible:
      if (features::UsePrefetchPrerenderIntegration()) {
        return PrefetchMatchResolverAction(
            PrefetchMatchResolverAction::ActionKind::kWait, load_state_,
            std::nullopt);
      } else {
        return PrefetchMatchResolverAction(
            PrefetchMatchResolverAction::ActionKind::kDrop, load_state_,
            std::nullopt);
      }
    case LoadState::kStarted:
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kWait, load_state_,
          std::nullopt);
    case LoadState::kDeterminedHead: {
      const bool is_expired = false;
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe, load_state_,
          is_expired);
    }
    case LoadState::kFailedDeterminedHead:
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kDrop, load_state_,
          std::nullopt);
    case LoadState::kCompleted: {
      CHECK(!redirect_chain_.empty());
      CHECK_EQ(redirect_chain_.back()->response_reader_->load_state(),
               PrefetchResponseReader::LoadState::kCompleted);
      // This branch corresponds to the first `if` in
      // `GetServableStateInternal()`.
      CHECK(GetNonRedirectResponseReader());
      const bool is_expired =
          !GetNonRedirectResponseReader()->Servable(cacheable_duration);
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kMaybeServe, load_state_,
          is_expired);
    }
    case LoadState::kFailedHeldback:
    case LoadState::kFailedIneligible:
    case LoadState::kFailed:
      return PrefetchMatchResolverAction(
          PrefetchMatchResolverAction::ActionKind::kDrop, load_state_,
          std::nullopt);
  }
}

PrefetchSingleRedirectHop&
PrefetchContainer::GetCurrentSingleRedirectHopToPrefetch() const {
  CHECK(redirect_chain_.size() > 0);
  return *redirect_chain_[redirect_chain_.size() - 1];
}

const PrefetchSingleRedirectHop&
PrefetchContainer::GetPreviousSingleRedirectHopToPrefetch() const {
  CHECK(redirect_chain_.size() > 1);
  return *redirect_chain_[redirect_chain_.size() - 2];
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
      request().prefetch_type().IsProxyRequiredWhenCrossOrigin());
  serving_page_metrics_container_->SetPrefetchHeaderLatency(
      GetPrefetchHeaderLatency());
  if (HasPrefetchStatus()) {
    serving_page_metrics_container_->SetPrefetchStatus(GetPrefetchStatus());
  }
}

void PrefetchContainer::SimulatePrefetchEligibleForTest() {
  if (request().attempt()) {
    request().attempt()->SetEligibility(PreloadingEligibility::kEligible);
    request().attempt()->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
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

  if (request().attempt()) {
    request().attempt()->SetEligibility(eligibility);
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

  if (base::FeatureList::IsEnabled(
          features::kPrefetchAsyncCancelOnCookiesChange)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&PrefetchContainer::CancelStreamingURLLoaderIfNotServing,
                       GetWeakPtr()));
  } else {
    CancelStreamingURLLoaderIfNotServing();
  }
}

void PrefetchContainer::OnPrefetchStarted() {
  SetLoadState(PrefetchContainer::LoadState::kStarted);
  prefetch_container_metrics_.time_prefetch_started = base::TimeTicks::Now();
}

GURL PrefetchContainer::GetCurrentURL() const {
  return GetCurrentSingleRedirectHopToPrefetch().url_;
}

GURL PrefetchContainer::GetPreviousURL() const {
  return GetPreviousSingleRedirectHopToPrefetch().url_;
}

bool PrefetchContainer::IsIsolatedNetworkContextRequiredForCurrentPrefetch()
    const {
  const PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToPrefetch();
  return this_prefetch.is_isolated_network_context_required_;
}

bool PrefetchContainer::IsIsolatedNetworkContextRequiredForPreviousRedirectHop()
    const {
  const PrefetchSingleRedirectHop& previous_prefetch =
      GetPreviousSingleRedirectHopToPrefetch();
  return previous_prefetch.is_isolated_network_context_required_;
}

base::WeakPtr<PrefetchResponseReader>
PrefetchContainer::GetResponseReaderForCurrentPrefetch() {
  const PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToPrefetch();
  CHECK(this_prefetch.response_reader_);
  return this_prefetch.response_reader_->GetWeakPtr();
}

bool PrefetchContainer::IsProxyRequiredForURL(const GURL& url) const {
  return IsCrossOriginRequest(url::Origin::Create(url)) &&
         request().prefetch_type().IsProxyRequiredWhenCrossOrigin();
}

void PrefetchContainer::MakeResourceRequest() {
  // |AddRedirectHop| updates this request later on. Anything here that should
  // be changed on redirect should happen there.

  const GURL& url = GetURL();
  url::Origin origin = url::Origin::Create(url);
  net::IsolationInfo isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, origin, origin,
      net::SiteForCookies::FromOrigin(origin));

  auto priority = [&] {
    if (request().priority().has_value()) {
      switch (request().priority().value()) {
        case PrefetchPriority::kLow:
          return net::RequestPriority::IDLE;
        case PrefetchPriority::kMedium:
          return net::RequestPriority::LOW;
        case PrefetchPriority::kHigh:
          return net::RequestPriority::MEDIUM;
        case PrefetchPriority::kHighest:
          return net::RequestPriority::HIGHEST;
      }
    }

    // TODO(crbug.com/426404355): Migrate to use `PrefetchPriority`.
    if (IsSpeculationRuleType(request().prefetch_type().trigger_type())) {
      // This may seem inverted (surely immediate prefetches would be higher
      // priority), but the fact that we're doing this at all for more
      // conservative candidates suggests a strong engagement signal.
      //
      // TODO(crbug.com/40276985): Ideally, we would actually use a combination
      // of the actual engagement seen (rather than the minimum required to
      // trigger the candidate) and the declared eagerness, and update them as
      // the prefetch becomes increasingly likely.
      blink::mojom::SpeculationEagerness eagerness =
          request().prefetch_type().GetEagerness();
      switch (eagerness) {
        case blink::mojom::SpeculationEagerness::kConservative:
          return net::RequestPriority::MEDIUM;
        case blink::mojom::SpeculationEagerness::kModerate:
          return net::RequestPriority::LOW;
        // TODO(crbug.com/40287486, crbug.com/406927300): Set appropriate value
        // after changing the behavior for `kEager`
        case blink::mojom::SpeculationEagerness::kEager:
        case blink::mojom::SpeculationEagerness::kImmediate:
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

  auto resource_request = CreateResourceRequestForNavigation(
      net::HttpRequestHeaders::kGetMethod, url,
      network::mojom::RequestDestination::kDocument, referrer_, isolation_info,
      std::move(devtools_observer_remote), priority, is_main_frame);

  // Note: Even without LOAD_DISABLE_CACHE, a cross-site prefetch uses a
  // separate network context, which means responses cached before the prefetch
  // are not visible to the prefetch, and anything cached by this request will
  // not be visible outside of the network context.
  resource_request->load_flags = net::LOAD_PREFETCH;

  // TODO(crbug.com/455296998): Remove this code for M145.
  if (request().should_bypass_http_cache()) {
    resource_request->load_flags |= net::LOAD_DISABLE_CACHE;
  }

  AddAwAdditionalHeaders(resource_request->headers,
                         request().additional_headers());

  CHECK(request().browser_context());
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAccept,
      FrameAcceptHeaderValue(/*allow_sxg_responses=*/true,
                             request().browser_context()));
  if (!base::FeatureList::IsEnabled(
          blink::features::kRemovePurposeHeaderForPrefetch)) {
    resource_request->headers.SetHeader(blink::kPurposeHeaderName,
                                        blink::kSecPurposePrefetchHeaderValue);
  }
  resource_request->headers.SetHeader(blink::kSecPurposeHeaderName,
                                      GetSecPurposeHeaderValue(url));
  resource_request->headers.SetHeader("Upgrade-Insecure-Requests", "1");

  // Sec-Speculation-Tags is set only when the prefetch is triggered
  // by speculation rules and it is not cross-site prefetch.
  // To see more details:
  // https://github.com/WICG/nav-speculation/blob/main/speculation-rules-tags.md#the-cross-site-case
  if (request().speculation_rules_tags().has_value() &&
      !IsCrossSiteRequest(origin)) {
    std::optional<std::string> serialized_list =
        request().speculation_rules_tags()->ConvertStringToHeaderString();
    CHECK(serialized_list.has_value());
    resource_request->headers.SetHeader(blink::kSecSpeculationTagsHeaderName,
                                        serialized_list.value());
  }

  // There are sometimes other headers that are set during navigation.  These
  // aren't yet supported for prefetch, including browsing topics.

  resource_request->devtools_request_id = RequestId();

  // TODO(crbug.com/444065296): These are an initial guess. Validate them
  // against the actual navigation's header.
  MaybeApplyOverrideForUserAgentHeader(*resource_request);
  AddClientHintsHeaders(origin, &resource_request->headers);
  if (request().should_append_variations_header()) {
    AddXClientDataHeader(*resource_request.get());
  }

  // `URLLoaderNetworkServiceObserver`
  // (`resource_request->trusted_params->url_loader_network_observer`) is NOT
  // set here, because for prefetching request we don't want to ask users e.g.
  // for authentication/cert errors, and instead make the prefetch fail. Because
  // of this, `ServiceWorkerClient::GetOngoingNavigationRequestBeforeCommit()`
  // is never called. `NavPrefetchBrowserTest` has the corresponding test
  // coverage.

  // Prefetches with `skip_service_worker` == `true` shouldn't serve navigation
  // with `skip_service_worker` == `false`, but right now we don't support such
  // prefetches.
  // TODO(https://crbug.com/438478667): Revisit this.
  CHECK(!resource_request->skip_service_worker);

  resource_request_ = std::move(resource_request);
}

void PrefetchContainer::UpdateReferrer(
    const GURL& new_referrer_url,
    const network::mojom::ReferrerPolicy& new_referrer_policy) {
  referrer_.url = new_referrer_url;
  referrer_.policy = new_referrer_policy;
}

const PrefetchKey& PrefetchContainer::key() const {
  return request().key();
}

const GURL& PrefetchContainer::GetURL() const {
  return request().key().url();
}

const std::optional<net::HttpNoVarySearchData>&
PrefetchContainer::GetNoVarySearchHint() const {
  return request().no_vary_search_hint();
}

bool PrefetchContainer::ShouldApplyUserAgentOverride(
    const GURL& request_url) const {
  if (!base::FeatureList::IsEnabled(
          features::kPreloadingRespectUserAgentOverride)) {
    return false;
  }

  WebContents* referring_web_contents =
      request().referring_web_contents().get();
  if (!referring_web_contents) {
    return false;
  }
  // The empty `ua_string_override` means no registered UA overrides.
  if (const blink::UserAgentOverride& ua_override =
          referring_web_contents->GetUserAgentOverride();
      ua_override.ua_string_override.empty()) {
    return false;
  }
  raw_ptr<WebContentsDelegate> delegate = referring_web_contents->GetDelegate();
  NavigationController::UserAgentOverrideOption option =
      delegate ? delegate->ShouldOverrideUserAgentForPreloading(request_url)
               : NavigationController::UA_OVERRIDE_INHERIT;
  // Use the primary main frame of initiator's WebContents to guess if we should
  // apply UA overrides in this prefetch request. Note that this decision is
  // independent with that of policy checking on ClientHints headers. This is an
  // estimation, i.e., can lead to wrong choices in some cases (e.g., where the
  // prefetched result is used in prerender for another WebContents).
  // TODO(crbug.com/444065296): Update this comment after the header comparison
  // between prefetch and prerender is implemented.
  auto* render_frame_host = referring_web_contents->GetPrimaryMainFrame();
  CHECK(render_frame_host);
  auto& nav_controller = static_cast<NavigationControllerImpl&>(
      render_frame_host->GetController());
  return nav_controller.ShouldOverrideUserAgentInNextNavigation(option);
}

void PrefetchContainer::MaybeApplyOverrideForUserAgentHeader(
    network::ResourceRequest& resource_request) {
  if (!ShouldApplyUserAgentOverride(resource_request.url)) {
    return;
  }
  WebContents* referring_web_contents =
      request_->referring_web_contents().get();
  if (!referring_web_contents) {
    return;
  }
  // TODO(crbug.com/444065296): This is an initial guess, because e.g.
  // `referring_web_contents` might be different from the navigation target's
  // WebContents. Validate this against the actual navigation's header.
  const blink::UserAgentOverride& ua_override =
      referring_web_contents->GetUserAgentOverride();
  CHECK(!ua_override.ua_string_override.empty());
  resource_request.headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                     ua_override.ua_string_override);
}

void PrefetchContainer::AddClientHintsHeaders(
    const url::Origin& origin,
    net::HttpRequestHeaders* request_headers) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchClientHints)) {
    return;
  }
  if (!request().browser_context()) {
    return;
  }
  ClientHintsControllerDelegate* client_hints_delegate =
      request().browser_context()->GetClientHintsControllerDelegate();
  if (!client_hints_delegate) {
    return;
  }

  auto* referring_ftn = base::FeatureList::IsEnabled(
                            features::kPrefetchDevtoolsUserAgentOverride) &&
                                request().referring_web_contents()
                            ? FrameTreeNode::From(RenderFrameHostImpl::FromID(
                                  request()
                                      .referring_web_contents()
                                      ->GetPrimaryMainFrame()
                                      ->GetGlobalId()))
                            : nullptr;

  // TODO(crbug.com/41497015): Consider supporting UA override mode here.
  const bool is_ua_override_on = false;
  net::HttpRequestHeaders client_hints_headers;
  if (request().is_javascript_enabled()) {
    // Add Client Hints headers
    //
    // Historically, `AddClientHintsHeadersToPrefetchNavigation` added
    // Client Hints headers iff `request().is_javascript_enabled()`, so the `if`
    // block here is to persist the behavior.
    // TODO(crbug.com/394716357): Revisit if we really want to allow prefetch
    // for non-Javascript enabled profile/origins.
    //
    // The request headers added by `referring_ftn` is the initial guess for the
    // request headers that will be used in the navigations served by this
    // prefetch, and can be different from the navigation target's
    // `FrameTreeNode` (crbug.com/444065296).
    // TODO(crbug.com/444065296): Validate the Client Hint headers added here
    // using `referring_ftn` against the navigation request's headers.
    AddClientHintsHeadersToPrefetchNavigation(
        origin, &client_hints_headers, request().browser_context(),
        client_hints_delegate, is_ua_override_on, referring_ftn);

    // This is an initial guess (crbug.com/444065296), e.g. ideally, the
    // DevTools UA overrides of the navigation target FrameTreeNode should be
    // used, but this is not available at the time of prefetch, so we use the
    // prefetch initiator's FrameTreeNode instead as an initial guess.
    // TODO(crbug.com/444065296): Validate the header against the actual
    // navigation's request header.
    //
    // For now, we only apply a part of
    // `devtools_instrumentation::ApplyNetworkRequestOverrides()` which is
    // applied to navigational request in
    // `NavigationRequest::OnStartChecksComplete()`.
    if (base::FeatureList::IsEnabled(
            features::kPrefetchDevtoolsUserAgentOverride) &&
        referring_ftn && RenderFrameDevToolsAgentHost::GetFor(referring_ftn)) {
      // Add/override `User-Agent` headers for DevTools emulation mode  by
      // `referring_ftn`'s devtools emulation mode.
      // TODO(crbug.com/422193319): This part only addresses devtools emulation
      // mode UA override. There are other types of UA overrides, which are at
      // WebContents level.
      devtools_instrumentation::ApplyEmulationOverrides(
          RenderFrameDevToolsAgentHost::GetFor(referring_ftn),
          &client_hints_headers);
    }
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
    case PrefetchContainer::LoadState::kDeterminedHead:
      return ostream << "DeterminedHead";
    case PrefetchContainer::LoadState::kFailedDeterminedHead:
      return ostream << "FailedDeterminedHead";
    case PrefetchContainer::LoadState::kCompleted:
      return ostream << "Completed";
    case PrefetchContainer::LoadState::kFailed:
      return ostream << "Failed";
    case PrefetchContainer::LoadState::kFailedHeldback:
      return ostream << "FailedHeldback";
  }
}

const char* PrefetchContainer::GetSecPurposeHeaderValue(
    const GURL& request_url) const {
  switch (request().preload_pipeline_info().planned_max_preloading_type()) {
    case PreloadingType::kPrefetch:
      if (IsProxyRequiredForURL(request_url)) {
        return blink::kSecPurposePrefetchAnonymousClientIpHeaderValue;
      } else {
        return blink::kSecPurposePrefetchHeaderValue;
      }
    case PreloadingType::kPrerenderUntilScript:
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
  if (auto* browser_initiator_info = request().GetBrowserInitiatorInfo()) {
    if (auto* listener = browser_initiator_info->request_status_listener()) {
      listener->OnPrefetchStartFailedGeneric();
    }
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
      request().no_vary_search_hint();
  return !GetNonRedirectHead() && no_vary_search_hint &&
         no_vary_search_hint->AreEquivalent(url, GetURL());
}

void PrefetchContainer::OnUnregisterCandidate(
    const GURL& navigated_url,
    bool is_served,
    PrefetchPotentialCandidateServingResult serving_result,
    bool is_nav_prerender,
    std::optional<base::TimeDelta> blocked_duration) {
  // Note that this method can be called with `is_in_dtor_` true.
  //
  // TODO(crbug.com/356314759): Avoid calling this with `is_in_dtor_`
  // true.

  if (is_served) {
    served_count_++;

    UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.AfterClick.RedirectChainSize",
                             redirect_chain_.size());
  }

  RecordPrefetchMatchingBlockedNavigationHistogram(blocked_duration.has_value(),
                                                   is_nav_prerender);

  RecordBlockUntilHeadDurationHistogram(blocked_duration, is_served,
                                        is_nav_prerender);

  RecordPrefetchPotentialCandidateServingResultHistogram(serving_result);

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
  if (request().attempt()) {
    static_cast<PreloadingAttemptImpl*>(request().attempt())
        ->SetIsAccurateTriggering(navigated_url);
  }
}

void PrefetchContainer::MergeNewPrefetchRequest(
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  // Propagate eligibility (and status) to `prefetch_request`.
  //
  // Assume we don't. (*) case is problematic.
  //
  // - If eligibility is not got, eligibility and status will be propagated by
  //   the following `OnEligibilityCheckComplete()` and
  //   `SetPrefetchStatusWithoutUpdatingTriggeringOutcome()`.
  // - If eligibility is got and ineligible, this `PrefetchContainer` is
  //   `kNotServed` and `MergeNewPrefetchRequest()` is not called.
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
  //     and `MergeNewPrefetchRequest()` is not called.
  //
  // In (*), `PrerenderHost` have to cancel prerender with eligibility
  // `kUnspecified` and status failure. It's relatively complicated condition.
  // See a test
  // `PrerendererImplBrowserTestPrefetchAhead.PrefetchMigratedPrefetchFailurePrerenderFailure`.
  //
  // To make things simple, we propagate both eligibility and status.
  scoped_refptr<PreloadPipelineInfoImpl> added_preload_pipeline_info =
      base::WrapRefCounted(&prefetch_request->preload_pipeline_info());

  added_preload_pipeline_info->SetPrefetchEligibility(
      request().preload_pipeline_info().prefetch_eligibility());
  if (auto prefetch_status =
          request().preload_pipeline_info().prefetch_status()) {
    added_preload_pipeline_info->SetPrefetchStatus(*prefetch_status);
  }

  inherited_preload_pipeline_infos_.push_back(
      std::move(added_preload_pipeline_info));

  is_likely_ahead_of_prerender_ |= CalculateIsLikelyAheadOfPrerender(
      prefetch_request->preload_pipeline_info());
}

void PrefetchContainer::NotifyPrefetchRequestWillBeSent(
    const network::mojom::URLResponseHeadPtr* redirect_head) {
  if (IsDecoy()) {
    return;
  }

  auto* renderer_initiator_info = request().GetRendererInitiatorInfo();
  if (!renderer_initiator_info) {
    // Don't emit CDP events if the trigger is not speculation rules.
    return;
  }

  auto* rfh = renderer_initiator_info->GetRenderFrameHost();
  auto* ftn = FrameTreeNode::From(rfh);
  if (!rfh) {
    // Don't emit CDP events if the initiator document isn't alive.
    return;
  }

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

  prefetch_container_metrics_.time_url_request_started =
      head.load_timing.request_start;
  prefetch_container_metrics_.time_domain_lookup_started =
      head.load_timing.connect_timing.domain_lookup_start;

  if (head.load_timing_internal_info.has_value()) {
    prefetch_container_metrics_.create_stream_delay =
        head.load_timing_internal_info->create_stream_delay;
    prefetch_container_metrics_.connected_callback_delay =
        head.load_timing_internal_info->connected_callback_delay;
    prefetch_container_metrics_.initialize_stream_delay =
        head.load_timing_internal_info->initialize_stream_delay;
  }

  // DevTools plumbing.
  auto* renderer_initiator_info = request().GetRendererInitiatorInfo();
  if (!renderer_initiator_info) {
    // Don't emit CDP events if the trigger is not speculation rules.
    return;
  }

  auto* ftn =
      FrameTreeNode::From(renderer_initiator_info->GetRenderFrameHost());
  if (!ftn) {
    // Don't emit CDP events if the initiator document isn't alive.
    return;
  }

  devtools_instrumentation::OnPrefetchResponseReceived(ftn, RequestId(),
                                                       GetCurrentURL(), head);
}

void PrefetchContainer::NotifyPrefetchRequestComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  // Ensured by the caller `PrefetchService::OnPrefetchResponseStarted()`.
  CHECK(!IsDecoy());

  auto* renderer_initiator_info = request().GetRendererInitiatorInfo();
  if (!renderer_initiator_info) {
    // Don't emit CDP events if the trigger is not speculation rules.
    return;
  }

  auto* ftn =
      FrameTreeNode::From(renderer_initiator_info->GetRenderFrameHost());
  if (!ftn) {
    // Don't emit CDP events if the initiator document isn't alive.
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

  auto* renderer_initiator_info = request().GetRendererInitiatorInfo();
  if (!renderer_initiator_info) {
    // Don't emit CDP events if the trigger is not speculation rules.
    return std::nullopt;
  }

  auto* ftn =
      FrameTreeNode::From(renderer_initiator_info->GetRenderFrameHost());
  if (!ftn) {
    // Don't emit CDP events if the initiator document isn't alive.
    return std::nullopt;
  }

  return NetworkServiceDevToolsObserver::MakeSelfOwned(ftn);
}

std::string PrefetchContainer::GetMetricsSuffix() const {
  std::optional<std::string> embedder_histogram_suffix;
  if (auto* browser_initiator_info = request().GetBrowserInitiatorInfo()) {
    embedder_histogram_suffix =
        browser_initiator_info->embedder_histogram_suffix();
  }
  return GetMetricsSuffixTriggerTypeAndEagerness(request().prefetch_type(),
                                                 embedder_histogram_suffix);
}

bool PrefetchContainer::HasPreloadPipelineInfoForMetrics(
    const PreloadPipelineInfo& other) const {
  if (&request().preload_pipeline_info() == &other) {
    return true;
  }

  for (const auto& preload_pipeline_info : inherited_preload_pipeline_infos_) {
    if (preload_pipeline_info.get() == &other) {
      return true;
    }
  }

  return false;
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

void PrefetchContainer::RecordPrefetchDurationHistogram() {
  if (!prefetch_container_metrics_.time_added_to_prefetch_service.has_value()) {
    return;
  }

  if (!prefetch_container_metrics_.time_initial_eligibility_got.has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToInitialEligibility.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_initial_eligibility_got.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  if (!prefetch_container_metrics_.time_prefetch_started.has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToPrefetchStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_prefetch_started.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.InitialEligibilityToPrefetchStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_prefetch_started.value() -
          prefetch_container_metrics_.time_initial_eligibility_got.value());

  if (!prefetch_container_metrics_.time_url_request_started.has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToURLRequestStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_url_request_started.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.PrefetchStartedToURLRequestStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_url_request_started.value() -
          prefetch_container_metrics_.time_prefetch_started.value());

  CHECK(prefetch_container_metrics_.time_domain_lookup_started.has_value());
  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToDomainLookupStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_domain_lookup_started.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());
  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.PrefetchStartedToDomainLookupStarted.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_domain_lookup_started.value() -
          prefetch_container_metrics_.time_prefetch_started.value());

  if (prefetch_container_metrics_.create_stream_delay.has_value()) {
    base::UmaHistogramTimes(base::StrCat({
                                "Prefetch.PrefetchContainer.CreateStreamDelay.",
                                GetMetricsSuffix(),
                            }),
                            *prefetch_container_metrics_.create_stream_delay);
  }
  if (prefetch_container_metrics_.connected_callback_delay.has_value()) {
    base::UmaHistogramTimes(
        base::StrCat({
            "Prefetch.Prefetchcontainer.ConnectedCallbackDelay.",
            GetMetricsSuffix(),
        }),
        *prefetch_container_metrics_.connected_callback_delay);
  }
  if (prefetch_container_metrics_.initialize_stream_delay) {
    base::UmaHistogramTimes(
        base::StrCat({
            "Prefetch.Prefetchcontainer.InitializeStreamDelay.",
            GetMetricsSuffix(),
        }),
        *prefetch_container_metrics_.initialize_stream_delay);
  }

  if (!prefetch_container_metrics_.time_header_determined_successfully
           .has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToHeaderDeterminedSuccessfully.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_header_determined_successfully.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer."
          "PrefetchStartedToHeaderDeterminedSuccessfully.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_header_determined_successfully.value() -
          prefetch_container_metrics_.time_prefetch_started.value());

  if (!prefetch_container_metrics_.time_prefetch_completed_successfully
           .has_value()) {
    return;
  }

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer.AddedToPrefetchCompletedSuccessfully.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_prefetch_completed_successfully.value() -
          prefetch_container_metrics_.time_added_to_prefetch_service.value());

  base::UmaHistogramTimes(
      base::StrCat({
          "Prefetch.PrefetchContainer."
          "PrefetchStartedToPrefetchCompletedSuccessfully.",
          GetMetricsSuffix(),
      }),
      prefetch_container_metrics_.time_prefetch_completed_successfully.value() -
          prefetch_container_metrics_.time_prefetch_started.value());
}

void PrefetchContainer::RecordPrefetchMatchMissedToPrefetchStartedHistogram() {
  if (prefetch_container_metrics_.time_prefetch_started.has_value() &&
      time_prefetch_match_missed_.has_value()) {
    base::UmaHistogramTimes(
        base::StrCat({
            "Prefetch.PrefetchContainer.PrefetchMatchMissedToPrefetchStarted.",
            GetMetricsSuffix(),
        }),
        prefetch_container_metrics_.time_prefetch_started.value() -
            time_prefetch_match_missed_.value());
  }
}

void PrefetchContainer::RecordPrefetchMatchingBlockedNavigationHistogram(
    bool blocked_until_head,
    bool is_nav_prerender) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.",
           GetMetricsSuffix()}),
      blocked_until_head);
  base::UmaHistogramBoolean(
      base::StrCat(
          {"Prefetch.PrefetchMatchingBlockedNavigation.PerMatchingCandidate.",
           is_nav_prerender ? "Prerender." : "NonPrerender.",
           GetMetricsSuffix()}),
      blocked_until_head);
}

void PrefetchContainer::RecordBlockUntilHeadDurationHistogram(
    const std::optional<base::TimeDelta>& blocked_duration,
    bool served,
    bool is_nav_prerender) {
  base::UmaHistogramTimes(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.",
                    served ? "Served." : "NotServed.", GetMetricsSuffix()}),
      blocked_duration.value_or(base::Seconds(0)));
  base::UmaHistogramTimes(
      base::StrCat({"Prefetch.BlockUntilHeadDuration.PerMatchingCandidate.",
                    is_nav_prerender ? "Prerender." : "NonPrerender.",
                    served ? "Served." : "NotServed.", GetMetricsSuffix()}),
      blocked_duration.value_or(base::Seconds(0)));
}

void PrefetchContainer::RecordPrefetchPotentialCandidateServingResultHistogram(
    PrefetchPotentialCandidateServingResult serving_result) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Prefetch.PrefetchPotentialCandidateServingResult."
                    "PerMatchingCandidate.",
                    GetMetricsSuffix()}),
      serving_result);
}

void PrefetchContainer::RecordPrefetchContainerServedCountHistogram() {
  base::UmaHistogramCounts100(
      base::StrCat(
          {"Prefetch.PrefetchContainer.ServedCount.", GetMetricsSuffix()}),
      served_count_);
}

}  // namespace content
