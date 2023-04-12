// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_page_metrics_container.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/proxy_lookup_client_impl.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
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

static_assert(
    static_cast<int>(PrefetchStatus::kMaxValue) +
        static_cast<int>(
            PreloadingEligibility::kPreloadingEligibilityCommonEnd) <=
    static_cast<int>(PreloadingEligibility::kPreloadingEligibilityContentEnd));

PreloadingEligibility ToPreloadingEligibility(PrefetchStatus status) {
  if (status == PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled) {
    return PreloadingEligibility::kDataSaverEnabled;
  }
  return static_cast<PreloadingEligibility>(
      static_cast<int>(status) +
      static_cast<int>(PreloadingEligibility::kPreloadingEligibilityCommonEnd));
}

// Please follow go/preloading-dashboard-updates if a new eligibility is added.
void SetIneligibilityFromStatus(PreloadingAttempt* attempt,
                                PrefetchStatus status) {
  if (attempt) {
    switch (status) {
      case PrefetchStatus::kPrefetchNotEligibleBrowserContextOffTheRecord:
      case PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
      case PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique:
      case PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
      case PrefetchStatus::kPrefetchProxyNotAvailable:
      case PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition:
      case PrefetchStatus::kPrefetchIneligibleRetryAfter:
      case PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker:
      case PrefetchStatus::kPrefetchNotEligibleUserHasCookies:
      case PrefetchStatus::kPrefetchNotEligibleExistingProxy:
        attempt->SetEligibility(ToPreloadingEligibility(status));
        break;
      default:
        NOTIMPLEMENTED();
    }
  }
}

static_assert(
    static_cast<int>(PrefetchStatus::kMaxValue) +
        static_cast<int>(
            PreloadingFailureReason::kPreloadingFailureReasonCommonEnd) <=
    static_cast<int>(
        PreloadingFailureReason::kPreloadingFailureReasonContentEnd));

PreloadingFailureReason ToPreloadingFailureReason(PrefetchStatus status) {
  return static_cast<PreloadingFailureReason>(
      static_cast<int>(status) +
      static_cast<int>(
          PreloadingFailureReason::kPreloadingFailureReasonCommonEnd));
}

// Please follow go/preloading-dashboard-updates if a new outcome enum or a
// failure reason enum is added.
void SetTriggeringOutcomeAndFailureReasonFromStatus(
    const absl::optional<base::UnguessableToken>&
        initiator_devtools_navigation_token,
    PreloadingAttempt* attempt,
    FrameTreeNode* ftn,
    const GURL& url,
    absl::optional<PrefetchStatus> old_prefetch_status,
    PrefetchStatus new_prefetch_status) {
  if (old_prefetch_status &&
      (old_prefetch_status.value() == PrefetchStatus::kPrefetchUsedNoProbe ||
       old_prefetch_status.value() == PrefetchStatus::kPrefetchResponseUsed)) {
    // Skip this update if the triggering outcome has already been updated
    // to kSuccess.
    return;
  }

  if (attempt) {
    switch (new_prefetch_status) {
      case PrefetchStatus::kPrefetchNotFinishedInTime:
        if (initiator_devtools_navigation_token.has_value()) {
          devtools_instrumentation::DidUpdatePrefetchStatus(
              ftn, initiator_devtools_navigation_token.value(), url,
              PreloadingTriggeringOutcome::kRunning);
        }
        attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kRunning);
        break;
      case PrefetchStatus::kPrefetchSuccessful:
        // A successful prefetch means the response is ready to be used for the
        // next navigation.
        if (initiator_devtools_navigation_token.has_value()) {
          devtools_instrumentation::DidUpdatePrefetchStatus(
              ftn, initiator_devtools_navigation_token.value(), url,
              PreloadingTriggeringOutcome::kReady);
        }
        attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kReady);
        break;
      case PrefetchStatus::kPrefetchUsedNoProbe:
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

        if (initiator_devtools_navigation_token.has_value()) {
          devtools_instrumentation::DidUpdatePrefetchStatus(
              ftn, initiator_devtools_navigation_token.value(), url,
              PreloadingTriggeringOutcome::kSuccess);
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
        if (initiator_devtools_navigation_token.has_value()) {
          devtools_instrumentation::DidUpdatePrefetchStatus(
              ftn, initiator_devtools_navigation_token.value(), url,
              PreloadingTriggeringOutcome::kFailure);
        }
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
      case PrefetchStatus::kPrefetchNotEligibleGoogleDomain:
      case PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker:
      case PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
      case PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition:
      case PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique:
      case PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
      case PrefetchStatus::kPrefetchNotEligibleExistingProxy:
      case PrefetchStatus::kPrefetchNotEligibleUserHasCookies:
      case PrefetchStatus::kPrefetchIneligibleRetryAfter:
      case PrefetchStatus::kPrefetchProxyNotAvailable:
      case PrefetchStatus::kPrefetchNotEligibleBrowserContextOffTheRecord:
      case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
      case PrefetchStatus::kPrefetchIsStale:
      case PrefetchStatus::kPrefetchNotUsedProbeFailed:
      case PrefetchStatus::kNavigatedToLinkNotOnSRP:
      case PrefetchStatus::kPrefetchUsedNoProbeWithNSP:
      case PrefetchStatus::kPrefetchUsedProbeSuccessWithNSP:
      case PrefetchStatus::kPrefetchNotUsedProbeFailedWithNSP:
      case PrefetchStatus::kPrefetchUsedNoProbeNSPAttemptDenied:
      case PrefetchStatus::kPrefetchUsedProbeSuccessNSPAttemptDenied:
      case PrefetchStatus::kPrefetchNotUsedProbeFailedNSPAttemptDenied:
      case PrefetchStatus::kPrefetchUsedNoProbeNSPNotStarted:
      case PrefetchStatus::kPrefetchUsedProbeSuccessNSPNotStarted:
      case PrefetchStatus::kPrefetchNotUsedProbeFailedNSPNotStarted:
      case PrefetchStatus::kPrefetchIsStaleWithNSP:
      case PrefetchStatus::kPrefetchIsStaleNSPAttemptDenied:
      case PrefetchStatus::kPrefetchIsStaleNSPNotStarted:
      case PrefetchStatus::kSubresourceThrottled:
      case PrefetchStatus::kPrefetchPositionIneligible:
      case PrefetchStatus::kPrefetchFailedRedirectsDisabled_DEPRECATED:
        NOTIMPLEMENTED();
    }
  }
}

std::string GetEagernessHistogramSuffix(
    const blink::mojom::SpeculationEagerness& eagerness) {
  switch (eagerness) {
    case blink::mojom::SpeculationEagerness::kEager:
      return "Eager";
    case blink::mojom::SpeculationEagerness::kModerate:
      return "Moderate";
    case blink::mojom::SpeculationEagerness::kConservative:
      return "Conservative";
  }
}

void RecordWasBlockedUntilHeadWhenServingHistogram(
    const blink::mojom::SpeculationEagerness& eagerness,
    bool blocked_until_head) {
  base::UmaHistogramBoolean(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          GetEagernessHistogramSuffix(eagerness).c_str()),
      blocked_until_head);
}

void RecordBlockUntilHeadDurationHistogram(
    const blink::mojom::SpeculationEagerness& eagerness,
    const base::TimeDelta& block_until_head_duration,
    bool served) {
  base::UmaHistogramTimes(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.BlockUntilHeadDuration.%s.%s",
          served ? "Served" : "NotServed",
          GetEagernessHistogramSuffix(eagerness).c_str()),
      block_until_head_duration);
}

}  // namespace

PrefetchContainer::PrefetchContainer(
    const GlobalRenderFrameHostId& referring_render_frame_host_id,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    absl::optional<net::HttpNoVarySearchData> no_vary_search_expected,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      prefetch_url_(url),
      prefetch_type_(prefetch_type),
      referrer_(referrer),
      no_vary_search_expected_(std::move(no_vary_search_expected)),
      prefetch_document_manager_(prefetch_document_manager),
      ukm_source_id_(prefetch_document_manager_
                         ? prefetch_document_manager_->render_frame_host()
                               .GetPageUkmSourceId()
                         : ukm::kInvalidSourceId),
      request_id_(base::UnguessableToken::Create().ToString()),
      initiator_devtools_navigation_token_(
          prefetch_document_manager
              ? prefetch_document_manager->initiator_devtools_navigation_token()
              : absl::nullopt) {
  auto* rfh = RenderFrameHost::FromID(referring_render_frame_host_id_);
  if (rfh) {
    auto* preloading_data = PreloadingData::GetOrCreateForWebContents(
        WebContents::FromRenderFrameHost(rfh));
    auto matcher =
        base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch)
            ? PreloadingDataImpl::GetSameURLAndNoVarySearchURLMatcher(
                  prefetch_document_manager_, prefetch_url_)
            : PreloadingDataImpl::GetSameURLMatcher(prefetch_url_);
    auto* attempt = preloading_data->AddPreloadingAttempt(
        content_preloading_predictor::kSpeculationRules,
        PreloadingType::kPrefetch, std::move(matcher));
    attempt_ = attempt->GetWeakPtr();
    // `PreloadingPrediction` is added in `PreloadingDecider`.
  }

  redirect_chain_.push_back(std::make_unique<SinglePrefetch>(prefetch_url_));
}

PrefetchContainer::~PrefetchContainer() {
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

  // TODO(https://crbug.com/1299059): Get the navigation start time and set the
  // NavigationStartToFetchStartMs field of the PrefetchProxy.PrefetchedResource
  // UKM event.

  builder.Record(ukm::UkmRecorder::Get());
}

void PrefetchContainer::SetPrefetchStatus(PrefetchStatus prefetch_status) {
  FrameTreeNode* ftn = FrameTreeNode::From(
      RenderFrameHostImpl::FromID(referring_render_frame_host_id_));
  SetTriggeringOutcomeAndFailureReasonFromStatus(
      initiator_devtools_navigation_token_, attempt_.get(), ftn, prefetch_url_,
      /*old_prefetch_status=*/prefetch_status_,
      /*new_prefetch_status=*/prefetch_status);
  prefetch_status_ = prefetch_status;
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

PrefetchNetworkContext* PrefetchContainer::GetOrCreateNetworkContext(
    PrefetchService* prefetch_service) {
  if (!network_context_) {
    network_context_ = std::make_unique<PrefetchNetworkContext>(
        prefetch_service, prefetch_type_, referrer_,
        referring_render_frame_host_id_);
  }
  return network_context_.get();
}

PrefetchDocumentManager* PrefetchContainer::GetPrefetchDocumentManager() const {
  return prefetch_document_manager_.get();
}

void PrefetchContainer::OnEligibilityCheckComplete(
    const GURL& url,
    bool is_eligible,
    absl::optional<PrefetchStatus> status) {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  DCHECK(this_prefetch);
  this_prefetch->is_eligible_ = is_eligible;
  if (this_prefetch->on_eligibility_check_complete_callback_) {
    std::move(this_prefetch->on_eligibility_check_complete_callback_)
        .Run(is_eligible);
  }

  if (url == prefetch_url_ && redirect_chain_.size() == 1) {
    // This case is for just the URL that was originally requested to be
    // prefetched.
    if (!is_eligible) {
      // Expect a reason (status) if not eligible.
      DCHECK(status.has_value());
      prefetch_status_ = status;
    }

    if (attempt_) {
      if (is_eligible) {
        attempt_->SetEligibility(PreloadingEligibility::kEligible);
      } else {
        SetIneligibilityFromStatus(attempt_.get(), prefetch_status_.value());
      }
    }

    if (prefetch_document_manager_) {
      prefetch_document_manager_->OnEligibilityCheckComplete(is_eligible);
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
  return redirect_chain_[0]->is_eligible_
             ? redirect_chain_[0]->is_eligible_.value()
             : false;
}

void PrefetchContainer::AddRedirectHop(const GURL& url) {
  redirect_chain_.push_back(std::make_unique<SinglePrefetch>(url));
}

absl::optional<bool> PrefetchContainer::GetEligibilityResultForRedirect(
    const GURL& url) {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  DCHECK(this_prefetch);
  return this_prefetch->is_eligible_;
}

void PrefetchContainer::SetOnEligibilityCheckCompleteCallback(
    const GURL& url,
    OnEligibilityCheckCompleteCallback on_eligibility_check_complete_callback) {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  DCHECK(this_prefetch);

  this_prefetch->on_eligibility_check_complete_callback_ =
      std::move(on_eligibility_check_complete_callback);
}

bool PrefetchContainer::IsOnEligibilityCheckCompleteCallbackRegistered(
    const GURL& url) const {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  DCHECK(this_prefetch);

  return !this_prefetch->on_eligibility_check_complete_callback_.is_null();
}

void PrefetchContainer::RegisterCookieListener(
    const GURL& url,
    network::mojom::CookieManager* cookie_manager) {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  DCHECK(this_prefetch);

  this_prefetch->cookie_listener_ = PrefetchCookieListener::MakeAndRegister(
      this_prefetch->url_, cookie_manager);
}

void PrefetchContainer::StopAllCookieListeners() {
  for (const auto& single_prefetch : redirect_chain_) {
    if (single_prefetch->cookie_listener_) {
      single_prefetch->cookie_listener_->StopListening();
    }
  }
}

bool PrefetchContainer::HaveDefaultContextCookiesChanged(
    const GURL& url) const {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  DCHECK(this_prefetch);

  if (this_prefetch->cookie_listener_) {
    return this_prefetch->cookie_listener_->HaveCookiesChanged();
  }
  return false;
}

bool PrefetchContainer::HasIsolatedCookieCopyStarted() const {
  switch (
      redirect_chain_[index_redirect_chain_to_serve_]->cookie_copy_status_) {
    case SinglePrefetch::CookieCopyStatus::kNotStarted:
      return false;
    case SinglePrefetch::CookieCopyStatus::kInProgress:
    case SinglePrefetch::CookieCopyStatus::kCompleted:
      return true;
  }
}

bool PrefetchContainer::IsIsolatedCookieCopyInProgress() const {
  switch (
      redirect_chain_[index_redirect_chain_to_serve_]->cookie_copy_status_) {
    case SinglePrefetch::CookieCopyStatus::kNotStarted:
    case SinglePrefetch::CookieCopyStatus::kCompleted:
      return false;
    case SinglePrefetch::CookieCopyStatus::kInProgress:
      return true;
  }
}

void PrefetchContainer::OnIsolatedCookieCopyStart() {
  DCHECK(!IsIsolatedCookieCopyInProgress());

  // We don't want any of the cookie listeners for this prefetch to pick up
  // changes from the copy.
  StopAllCookieListeners();

  redirect_chain_[index_redirect_chain_to_serve_]->cookie_copy_status_ =
      SinglePrefetch::CookieCopyStatus::kInProgress;

  redirect_chain_[index_redirect_chain_to_serve_]->cookie_copy_start_time_ =
      base::TimeTicks::Now();
}

void PrefetchContainer::OnIsolatedCookiesReadCompleteAndWriteStart() {
  DCHECK(IsIsolatedCookieCopyInProgress());

  redirect_chain_[index_redirect_chain_to_serve_]
      ->cookie_read_end_and_write_start_time_ = base::TimeTicks::Now();
}

void PrefetchContainer::OnIsolatedCookieCopyComplete() {
  DCHECK(IsIsolatedCookieCopyInProgress());

  const auto& this_prefetch = redirect_chain_[index_redirect_chain_to_serve_];

  this_prefetch->cookie_copy_status_ =
      SinglePrefetch::CookieCopyStatus::kCompleted;

  if (this_prefetch->cookie_copy_start_time_.has_value() &&
      this_prefetch->cookie_read_end_and_write_start_time_.has_value()) {
    RecordCookieCopyTimes(
        this_prefetch->cookie_copy_start_time_.value(),
        this_prefetch->cookie_read_end_and_write_start_time_.value(),
        base::TimeTicks::Now());
  }

  if (this_prefetch->on_cookie_copy_complete_callback_) {
    std::move(this_prefetch->on_cookie_copy_complete_callback_).Run();
  }
}

void PrefetchContainer::OnInterceptorCheckCookieCopy() {
  if (!redirect_chain_[index_redirect_chain_to_serve_]
           ->cookie_copy_start_time_) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      base::TimeTicks::Now() - redirect_chain_[index_redirect_chain_to_serve_]
                                   ->cookie_copy_start_time_.value(),
      base::TimeDelta(), base::Seconds(5), 50);
}

void PrefetchContainer::SetOnCookieCopyCompleteCallback(
    base::OnceClosure callback) {
  DCHECK(IsIsolatedCookieCopyInProgress());

  redirect_chain_[index_redirect_chain_to_serve_]
      ->on_cookie_copy_complete_callback_ = std::move(callback);
}

void PrefetchContainer::TakeStreamingURLLoader(
    std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader) {
  DCHECK(!streaming_loader_);
  streaming_loader_ = std::move(streaming_loader);
}

std::unique_ptr<PrefetchStreamingURLLoader>
PrefetchContainer::ReleaseStreamingLoader() {
  DCHECK(streaming_loader_);
  return std::move(streaming_loader_);
}

void PrefetchContainer::ResetStreamingLoader() {
  DCHECK(streaming_loader_);

  // The streaming URL loader can be deleted in one of its callbacks, so instead
  // of deleting it immediately, it is made self owned and then deletes itself.
  PrefetchStreamingURLLoader* raw_streaming_loader = streaming_loader_.get();
  raw_streaming_loader->MakeSelfOwnedAndDeleteSoon(
      std::move(streaming_loader_));
}

void PrefetchContainer::OnPrefetchProbeResult(
    PrefetchProbeResult probe_result) {
  probe_result_ = probe_result;

  switch (probe_result) {
    case PrefetchProbeResult::kNoProbing:
    case PrefetchProbeResult::kDNSProbeSuccess:
    case PrefetchProbeResult::kTLSProbeSuccess:
      // Wait to update the prefetch status until the probe for the final
      // redirect hop is a success.
      if (index_redirect_chain_to_serve_ == redirect_chain_.size() - 1) {
        SetPrefetchStatus(PrefetchStatus::kPrefetchResponseUsed);
      }
      break;
    case PrefetchProbeResult::kDNSProbeFailure:
    case PrefetchProbeResult::kTLSProbeFailure:
      prefetch_status_ = PrefetchStatus::kPrefetchNotUsedProbeFailed;
      break;
    default:
      NOTIMPLEMENTED();
  }
}

void PrefetchContainer::OnPrefetchedResponseHeadReceived() {
  if (prefetch_document_manager_) {
    prefetch_document_manager_->OnPrefetchedHeadReceived(GetURL());
  }
}

void PrefetchContainer::OnPrefetchComplete() {
  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.Prefetch.RedirectChainSize",
                           redirect_chain_.size());

  if (!streaming_loader_) {
    return;
  }

  UpdatePrefetchRequestMetrics(streaming_loader_->GetCompletionStatus(),
                               streaming_loader_->GetHead());
  UpdateServingPageMetrics();
}

void PrefetchContainer::UpdatePrefetchRequestMetrics(
    const absl::optional<network::URLLoaderCompletionStatus>& completion_status,
    const network::mojom::URLResponseHead* head) {
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

bool PrefetchContainer::ShouldBlockUntilHeadReceived() const {
  // Can only block until head if the request has been started using a streaming
  // URL loader and head hasn't been received yet.
  if (!streaming_loader_ || streaming_loader_->GetHead() ||
      streaming_loader_->Failed()) {
    return false;
  }
  return PrefetchShouldBlockUntilHead(prefetch_type_.GetEagerness());
}

bool PrefetchContainer::IsPrefetchServable(
    base::TimeDelta cacheable_duration) const {
  // Whether or not the response (either full or partial) from the streaming URL
  // loader is servable.
  return streaming_loader_ && streaming_loader_->Servable(cacheable_duration);
}

bool PrefetchContainer::DoesCurrentURLToServeMatch(const GURL& url) const {
  DCHECK(index_redirect_chain_to_serve_ >= 1 &&
         index_redirect_chain_to_serve_ < redirect_chain_.size());
  return redirect_chain_[index_redirect_chain_to_serve_]->url_ == url ||
         IsMatchingNoVarySearchUrl(
             redirect_chain_[index_redirect_chain_to_serve_]->url_, url);
}

const GURL& PrefetchContainer::GetCurrentURLToServe() const {
  DCHECK(index_redirect_chain_to_serve_ >= 0 &&
         index_redirect_chain_to_serve_ < redirect_chain_.size());
  return redirect_chain_[index_redirect_chain_to_serve_]->url_;
}

const network::mojom::URLResponseHead* PrefetchContainer::GetHead() {
  return streaming_loader_ ? streaming_loader_->GetHead() : nullptr;
}

void PrefetchContainer::SetServingPageMetrics(
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container) {
  serving_page_metrics_container_ = serving_page_metrics_container;
}

void PrefetchContainer::UpdateServingPageMetrics() {
  if (!serving_page_metrics_container_) {
    return;
  }

  serving_page_metrics_container_->SetRequiredPrivatePrefetchProxy(
      GetPrefetchType().IsProxyRequired());
  serving_page_metrics_container_->SetPrefetchHeaderLatency(
      GetPrefetchHeaderLatency());
  if (HasPrefetchStatus()) {
    serving_page_metrics_container_->SetPrefetchStatus(GetPrefetchStatus());
  }
}

void PrefetchContainer::SimulateAttemptAtInterceptorForTest() {
  if (attempt_) {
    attempt_->SetEligibility(PreloadingEligibility::kEligible);
    attempt_->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
  }
  SetPrefetchStatus(PrefetchStatus::kPrefetchAllowed);
  SetPrefetchStatus(PrefetchStatus::kPrefetchSuccessful);
}

PrefetchContainer::SinglePrefetch* PrefetchContainer::GetSinglePrefetch(
    const GURL& url) const {
  for (auto itr = redirect_chain_.rbegin(); itr != redirect_chain_.rend();
       itr++) {
    GURL single_prefetch_url = (*itr)->url_;
    if (single_prefetch_url == url ||
        IsMatchingNoVarySearchUrl(single_prefetch_url, url)) {
      return itr->get();
    }
  }
  NOTREACHED();
  return nullptr;
}

bool PrefetchContainer::IsMatchingNoVarySearchUrl(
    const GURL& internal_url,
    const GURL& external_url) const {
  if (!no_vary_search_helper_) {
    return false;
  }

  absl::optional<GURL> no_vary_search_match_url =
      no_vary_search_helper_->MatchUrl(external_url);
  if (!no_vary_search_match_url) {
    return false;
  }

  return internal_url == no_vary_search_match_url.value();
}

void PrefetchContainer::OnGetPrefetchToServe(bool blocked_until_head) {
  if (blocked_until_head) {
    blocked_until_head_start_time_ = base::TimeTicks::Now();
  }

  RecordWasBlockedUntilHeadWhenServingHistogram(prefetch_type_.GetEagerness(),
                                                blocked_until_head);
}

void PrefetchContainer::OnReturnPrefetchToServe(bool served) {
  if (served) {
    UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.AfterClick.RedirectChainSize",
                             redirect_chain_.size());
    navigated_to_ = true;
  }

  if (blocked_until_head_start_time_.has_value()) {
    RecordBlockUntilHeadDurationHistogram(
        prefetch_type_.GetEagerness(),
        base::TimeTicks::Now() - blocked_until_head_start_time_.value(),
        served);
  }
}

std::ostream& operator<<(std::ostream& ostream,
                         const PrefetchContainer& prefetch_container) {
  return ostream << "PrefetchContainer[" << &prefetch_container
                 << ", URL=" << prefetch_container.GetURL() << "]";
}

PrefetchContainer::SinglePrefetch::SinglePrefetch(const GURL& url)
    : url_(url) {}

PrefetchContainer::SinglePrefetch::~SinglePrefetch() = default;

}  // namespace content
