// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_container.h"

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/time/time.h"
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
#include "content/browser/preloading/prefetch/prefetched_mainframe_response_container.h"
#include "content/browser/preloading/prefetch/proxy_lookup_client_impl.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
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
    PreloadingAttempt* attempt,
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
        attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kRunning);
        break;
      case PrefetchStatus::kPrefetchSuccessful:
        // A successful prefetch means the response is ready to be used for the
        // next navigation.
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

        attempt->SetTriggeringOutcome(PreloadingTriggeringOutcome::kSuccess);
        break;
      // A decoy is considered eligible because a network request is made for
      // it. It is considered as a failure as the final response is never
      // served.
      case PrefetchStatus::kPrefetchIsPrivacyDecoy:
      case PrefetchStatus::kPrefetchFailedRedirectsDisabled:
      case PrefetchStatus::kPrefetchFailedNetError:
      case PrefetchStatus::kPrefetchFailedNon2XX:
      case PrefetchStatus::kPrefetchFailedMIMENotSupported:
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
        NOTIMPLEMENTED();
    }
  }
}

void SetHoldbackFromStatus(PreloadingAttempt* attempt, PrefetchStatus status) {
  if (attempt) {
    switch (status) {
      case PrefetchStatus::kPrefetchAllowed:
        attempt->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
        break;
      case PrefetchStatus::kPrefetchHeldback:
        attempt->SetHoldbackStatus(PreloadingHoldbackStatus::kHoldback);
        break;
      default:
        break;
    }
  }
}

}  // namespace

PrefetchContainer::PrefetchContainer(
    const GlobalRenderFrameHostId& referring_render_frame_host_id,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      url_(url),
      prefetch_type_(prefetch_type),
      referrer_(referrer),
      prefetch_document_manager_(prefetch_document_manager),
      ukm_source_id_(prefetch_document_manager_
                         ? prefetch_document_manager_->render_frame_host()
                               .GetPageUkmSourceId()
                         : ukm::kInvalidSourceId),
      request_id_(base::UnguessableToken::Create().ToString()) {
  auto* rfh = RenderFrameHost::FromID(referring_render_frame_host_id_);
  if (rfh) {
    auto* preloading_data = PreloadingData::GetOrCreateForWebContents(
        WebContents::FromRenderFrameHost(rfh));
    auto matcher =
        base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch)
            ? PreloadingDataImpl::GetSameURLAndNoVarySearchURLMatcher(
                  prefetch_document_manager_, url_)
            : PreloadingDataImpl::GetSameURLMatcher(url_);
    auto* attempt = preloading_data->AddPreloadingAttempt(
        content_preloading_predictor::kSpeculationRules,
        PreloadingType::kPrefetch, std::move(matcher));
    attempt_ = attempt->GetWeakPtr();
    // `PreloadingPrediction` is added in `PreloadingDecider`.
  }
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
  SetHoldbackFromStatus(attempt_.get(), prefetch_status);
  SetTriggeringOutcomeAndFailureReasonFromStatus(
      attempt_.get(),
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
    bool is_eligible,
    absl::optional<PrefetchStatus> status) {
  is_eligible_ = is_eligible;
  if (!is_eligible_) {
    // Expect a reason (status) if not eligible.
    DCHECK(status.has_value());
    prefetch_status_ = status;
    SetIneligibilityFromStatus(attempt_.get(), prefetch_status_.value());
  } else if (attempt_) {
    attempt_->SetEligibility(PreloadingEligibility::kEligible);
  }
  prefetch_document_manager_->OnEligibilityCheckComplete(is_eligible);
}

void PrefetchContainer::RegisterCookieListener(
    network::mojom::CookieManager* cookie_manager) {
  cookie_listener_ =
      PrefetchCookieListener::MakeAndRegister(url_, cookie_manager);
}

void PrefetchContainer::StopCookieListener() {
  if (cookie_listener_)
    cookie_listener_->StopListening();
}

bool PrefetchContainer::HaveDefaultContextCookiesChanged() const {
  if (cookie_listener_)
    return cookie_listener_->HaveCookiesChanged();
  return false;
}

bool PrefetchContainer::HasIsolatedCookieCopyStarted() const {
  switch (cookie_copy_status_) {
    case CookieCopyStatus::kNotStarted:
      return false;
    case CookieCopyStatus::kInProgress:
    case CookieCopyStatus::kCompleted:
      return true;
  }
}

bool PrefetchContainer::IsIsolatedCookieCopyInProgress() const {
  switch (cookie_copy_status_) {
    case CookieCopyStatus::kNotStarted:
    case CookieCopyStatus::kCompleted:
      return false;
    case CookieCopyStatus::kInProgress:
      return true;
  }
}

void PrefetchContainer::OnIsolatedCookieCopyStart() {
  DCHECK(!IsIsolatedCookieCopyInProgress());

  // We don't want the cookie listener for this URL to get the changes from the
  // copy.
  StopCookieListener();

  cookie_copy_status_ = CookieCopyStatus::kInProgress;

  cookie_copy_start_time_ = base::TimeTicks::Now();
}

void PrefetchContainer::OnIsolatedCookiesReadCompleteAndWriteStart() {
  DCHECK(IsIsolatedCookieCopyInProgress());

  cookie_read_end_and_write_start_time_ = base::TimeTicks::Now();
}

void PrefetchContainer::OnIsolatedCookieCopyComplete() {
  DCHECK(IsIsolatedCookieCopyInProgress());

  cookie_copy_status_ = CookieCopyStatus::kCompleted;

  if (cookie_copy_start_time_.has_value() &&
      cookie_read_end_and_write_start_time_.has_value()) {
    RecordCookieCopyTimes(*cookie_copy_start_time_,
                          *cookie_read_end_and_write_start_time_,
                          base::TimeTicks::Now());
  }

  if (on_cookie_copy_complete_callback_)
    std::move(on_cookie_copy_complete_callback_).Run();
}

void PrefetchContainer::OnInterceptorCheckCookieCopy() {
  if (!cookie_copy_start_time_)
    return;

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      base::TimeTicks::Now() - cookie_copy_start_time_.value(),
      base::TimeDelta(), base::Seconds(5), 50);
}

void PrefetchContainer::SetOnCookieCopyCompleteCallback(
    base::OnceClosure callback) {
  DCHECK(IsIsolatedCookieCopyInProgress());
  on_cookie_copy_complete_callback_ = std::move(callback);
}

void PrefetchContainer::TakeURLLoader(
    std::unique_ptr<network::SimpleURLLoader> loader) {
  DCHECK(!loader_);
  DCHECK(!PrefetchUseStreamingURLLoader());
  loader_ = std::move(loader);
}

void PrefetchContainer::ResetURLLoader() {
  DCHECK(loader_);
  DCHECK(!PrefetchUseStreamingURLLoader());
  loader_.reset();
}

void PrefetchContainer::TakeStreamingURLLoader(
    std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader) {
  DCHECK(!streaming_loader_);
  DCHECK(PrefetchUseStreamingURLLoader());
  streaming_loader_ = std::move(streaming_loader);
}

std::unique_ptr<PrefetchStreamingURLLoader>
PrefetchContainer::ReleaseStreamingLoader() {
  DCHECK(streaming_loader_);
  DCHECK(PrefetchUseStreamingURLLoader());
  return std::move(streaming_loader_);
}

void PrefetchContainer::ResetStreamingLoader() {
  DCHECK(streaming_loader_);
  DCHECK(PrefetchUseStreamingURLLoader());

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
      SetPrefetchStatus(PrefetchStatus::kPrefetchUsedNoProbe);
      break;
    case PrefetchProbeResult::kDNSProbeSuccess:
    case PrefetchProbeResult::kTLSProbeSuccess:
      SetPrefetchStatus(PrefetchStatus::kPrefetchResponseUsed);
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
  if (!loader_ && !streaming_loader_) {
    return;
  }

  absl::optional<network::URLLoaderCompletionStatus> completion_status;
  const network::mojom::URLResponseHead* head;
  if (loader_) {
    completion_status = loader_->CompletionStatus();
    head = loader_->ResponseInfo();
  } else if (streaming_loader_) {
    completion_status = streaming_loader_->GetCompletionStatus();
    head = streaming_loader_->GetHead();
  }

  UpdatePrefetchRequestMetrics(completion_status, head);
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
  if (!streaming_loader_ || streaming_loader_->GetHead()) {
    return false;
  }
  return PrefetchShouldBlockUntilHead(prefetch_type_.GetEagerness());
}

bool PrefetchContainer::IsPrefetchServable(
    base::TimeDelta cacheable_duration) const {
  // Whether or not the response (either full or partial) from the streaming URL
  // loader is servable.
  bool streaming_loader_servable =
      streaming_loader_ && streaming_loader_->Servable(cacheable_duration);

  // Whether or not there is a valid response from the non-streaming URL loader.
  bool valid_response =
      prefetched_response_ != nullptr && prefetch_received_time_.has_value() &&
      base::TimeTicks::Now() <
          prefetch_received_time_.value() + cacheable_duration;

  return streaming_loader_servable || valid_response;
}

void PrefetchContainer::TakePrefetchedResponse(
    std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response) {
  DCHECK(!prefetched_response_);
  DCHECK(!is_decoy_);

  prefetch_received_time_ = base::TimeTicks::Now();
  prefetched_response_ = std::move(prefetched_response);

  if (prefetch_document_manager_) {
    prefetch_document_manager_->OnPrefetchSuccessful();
  }
}

std::unique_ptr<PrefetchedMainframeResponseContainer>
PrefetchContainer::ReleasePrefetchedResponse() {
  prefetch_received_time_.reset();
  return std::move(prefetched_response_);
}

const network::mojom::URLResponseHead* PrefetchContainer::GetHead() {
  if (prefetched_response_) {
    return prefetched_response_->GetHead();
  }

  if (streaming_loader_) {
    return streaming_loader_->GetHead();
  }

  return nullptr;
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
  }
  SetPrefetchStatus(PrefetchStatus::kPrefetchAllowed);
  SetPrefetchStatus(PrefetchStatus::kPrefetchSuccessful);
}

}  // namespace content
