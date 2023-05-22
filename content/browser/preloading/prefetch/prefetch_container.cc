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
  switch (status) {
    case PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
      return PreloadingEligibility::kDataSaverEnabled;
    case PrefetchStatus::kPrefetchNotEligibleBatterySaverEnabled:
      return PreloadingEligibility::kBatterySaverEnabled;
    case PrefetchStatus::kPrefetchNotEligiblePreloadingDisabled:
      return PreloadingEligibility::kPreloadingDisabled;
    default:
      return static_cast<PreloadingEligibility>(
          static_cast<int>(status) +
          static_cast<int>(
              PreloadingEligibility::kPreloadingEligibilityCommonEnd));
  }
}

// Please follow go/preloading-dashboard-updates if a new eligibility is added.
void SetIneligibilityFromStatus(PreloadingAttempt* attempt,
                                PrefetchStatus status) {
  if (attempt) {
    switch (status) {
      case PrefetchStatus::kPrefetchNotEligibleBrowserContextOffTheRecord:
      case PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
      case PrefetchStatus::kPrefetchNotEligibleBatterySaverEnabled:
      case PrefetchStatus::kPrefetchNotEligiblePreloadingDisabled:
      case PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique:
      case PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
      case PrefetchStatus::kPrefetchProxyNotAvailable:
      case PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition:
      case PrefetchStatus::kPrefetchIneligibleRetryAfter:
      case PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker:
      case PrefetchStatus::kPrefetchNotEligibleUserHasCookies:
      case PrefetchStatus::kPrefetchNotEligibleExistingProxy:
      case PrefetchStatus::
          kPrefetchNotEligibleSameSiteCrossOriginPrefetchRequiredProxy:
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

absl::optional<PreloadingTriggeringOutcome> TriggeringOutcomeFromStatus(
    PrefetchStatus prefetch_status) {
  switch (prefetch_status) {
    case PrefetchStatus::kPrefetchNotFinishedInTime:
      return PreloadingTriggeringOutcome::kRunning;
    case PrefetchStatus::kPrefetchSuccessful:
      return PreloadingTriggeringOutcome::kReady;
    case PrefetchStatus::kPrefetchResponseUsed:
      return PreloadingTriggeringOutcome::kSuccess;
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchStatus::kPrefetchFailedNetError:
    case PrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
    case PrefetchStatus::kPrefetchFailedInvalidRedirect:
    case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
    case PrefetchStatus::kPrefetchFailedPerPageLimitExceeded:
    case PrefetchStatus::kPrefetchEvicted:
    case PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker:
    case PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
    case PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition:
    case PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique:
    case PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
    case PrefetchStatus::kPrefetchNotEligibleBatterySaverEnabled:
    case PrefetchStatus::kPrefetchNotEligiblePreloadingDisabled:
    case PrefetchStatus::kPrefetchNotEligibleExistingProxy:
    case PrefetchStatus::kPrefetchNotEligibleUserHasCookies:
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::kPrefetchNotEligibleBrowserContextOffTheRecord:
    case PrefetchStatus::
        kPrefetchNotEligibleSameSiteCrossOriginPrefetchRequiredProxy:
      return PreloadingTriggeringOutcome::kFailure;
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchAllowed:
    case PrefetchStatus::kPrefetchNotStarted:
    case PrefetchStatus::kPrefetchProxyNotAvailable:
      return absl::nullopt;
  }
  return absl::nullopt;
}

// Please follow go/preloading-dashboard-updates if a new outcome enum or a
// failure reason enum is added.
void SetTriggeringOutcomeAndFailureReasonFromStatus(
    PreloadingAttempt* attempt,
    const GURL& url,
    absl::optional<PrefetchStatus> old_prefetch_status,
    PrefetchStatus new_prefetch_status) {
  if (old_prefetch_status &&
      old_prefetch_status.value() == PrefetchStatus::kPrefetchResponseUsed) {
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
      case PrefetchStatus::kPrefetchFailedPerPageLimitExceeded:
      // TODO(adithyas): This would report 'eviction' as a failure even though
      // the initial prefetch succeeded, consider introducing a different
      // PreloadingTriggerOutcome for eviction.
      case PrefetchStatus::kPrefetchEvicted:
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
      case PrefetchStatus::kPrefetchNotEligibleUserHasServiceWorker:
      case PrefetchStatus::kPrefetchNotEligibleSchemeIsNotHttps:
      case PrefetchStatus::kPrefetchNotEligibleNonDefaultStoragePartition:
      case PrefetchStatus::kPrefetchNotEligibleHostIsNonUnique:
      case PrefetchStatus::kPrefetchNotEligibleDataSaverEnabled:
      case PrefetchStatus::kPrefetchNotEligibleBatterySaverEnabled:
      case PrefetchStatus::kPrefetchNotEligiblePreloadingDisabled:
      case PrefetchStatus::kPrefetchNotEligibleExistingProxy:
      case PrefetchStatus::kPrefetchNotEligibleUserHasCookies:
      case PrefetchStatus::kPrefetchIneligibleRetryAfter:
      case PrefetchStatus::kPrefetchProxyNotAvailable:
      case PrefetchStatus::kPrefetchNotEligibleBrowserContextOffTheRecord:
      case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
      case PrefetchStatus::kPrefetchIsStale:
      case PrefetchStatus::kPrefetchNotUsedProbeFailed:
      case PrefetchStatus::
          kPrefetchNotEligibleSameSiteCrossOriginPrefetchRequiredProxy:
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
    absl::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    blink::mojom::SpeculationInjectionWorld world,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      prefetch_url_(url),
      prefetch_type_(prefetch_type),
      referrer_(referrer),
      referring_origin_(url::Origin::Create(referrer_.url)),
      referring_site_(net::SchemefulSite(referrer_.url)),
      no_vary_search_hint_(std::move(no_vary_search_hint)),
      prefetch_document_manager_(prefetch_document_manager),
      ukm_source_id_(prefetch_document_manager_
                         ? prefetch_document_manager_->render_frame_host()
                               .GetPageUkmSourceId()
                         : ukm::kInvalidSourceId),
      request_id_(base::UnguessableToken::Create().ToString()) {
  auto* rfhi = RenderFrameHostImpl::FromID(referring_render_frame_host_id);
  // Note: |rfhi| is only nullptr in unit tests.
  if (rfhi) {
    auto* preloading_data = PreloadingData::GetOrCreateForWebContents(
        WebContents::FromRenderFrameHost(rfhi));
    auto matcher =
        base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch)
            ? PreloadingDataImpl::GetSameURLAndNoVarySearchURLMatcher(
                  prefetch_document_manager_, prefetch_url_)
            : PreloadingDataImpl::GetSameURLMatcher(prefetch_url_);
    auto* attempt = preloading_data->AddPreloadingAttempt(
        GetPredictorForSpeculationRules(world), PreloadingType::kPrefetch,
        std::move(matcher));
    attempt_ = attempt->GetWeakPtr();
    initiator_devtools_navigation_token_ = rfhi->GetDevToolsNavigationToken();
  }

  // `PreloadingPrediction` is added in `PreloadingDecider`.

  redirect_chain_.push_back(
      std::make_unique<SinglePrefetch>(prefetch_url_, referring_site_));
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

void PrefetchContainer::SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
    PrefetchStatus prefetch_status) {
  prefetch_status_ = prefetch_status;
  FrameTreeNode* ftn = FrameTreeNode::From(
      RenderFrameHostImpl::FromID(referring_render_frame_host_id_));

  absl::optional<PreloadingTriggeringOutcome> preloading_trigger_outcome =
      TriggeringOutcomeFromStatus(prefetch_status);

  if (initiator_devtools_navigation_token_.has_value() &&
      preloading_trigger_outcome.has_value()) {
    devtools_instrumentation::DidUpdatePrefetchStatus(
        ftn, initiator_devtools_navigation_token_.value(), prefetch_url_,
        preloading_trigger_outcome.value(), prefetch_status);
  }
}

void PrefetchContainer::SetPrefetchStatus(PrefetchStatus prefetch_status) {
  SetTriggeringOutcomeAndFailureReasonFromStatus(
      attempt_.get(), prefetch_url_,
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

PrefetchNetworkContext* PrefetchContainer::GetOrCreateNetworkContextForURL(
    const GURL& url,
    PrefetchService* prefetch_service) {
  bool is_isolated_network_context_required =
      IsIsolatedNetworkContextRequiredForURL(url);

  auto network_context_itr =
      network_contexts_.find(is_isolated_network_context_required);
  if (network_context_itr == network_contexts_.end()) {
    network_context_itr =
        network_contexts_
            .emplace(
                is_isolated_network_context_required,
                std::make_unique<PrefetchNetworkContext>(
                    prefetch_service, is_isolated_network_context_required,
                    prefetch_type_, referrer_, referring_render_frame_host_id_))
            .first;
  }

  CHECK(network_context_itr != network_contexts_.end());
  CHECK(network_context_itr->second);
  return network_context_itr->second.get();
}

PrefetchNetworkContext* PrefetchContainer::GetNetworkContextForURL(
    const GURL& url) const {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  CHECK(this_prefetch);

  const auto& network_context_itr = network_contexts_.find(
      this_prefetch->is_isolated_network_context_required_);
  if (network_context_itr == network_contexts_.end()) {
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

void PrefetchContainer::OnEligibilityCheckComplete(
    const GURL& url,
    bool is_eligible,
    absl::optional<PrefetchStatus> status) {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  DCHECK(this_prefetch);
  this_prefetch->is_eligible_ = is_eligible;

  if (url == prefetch_url_ && redirect_chain_.size() == 1) {
    // This case is for just the URL that was originally requested to be
    // prefetched.
    if (!is_eligible) {
      // Expect a reason (status) if not eligible.
      DCHECK(status.has_value());
      SetPrefetchStatusWithoutUpdatingTriggeringOutcome(status.value());
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
  redirect_chain_.push_back(
      std::make_unique<SinglePrefetch>(url, referring_site_));
}

absl::optional<bool> PrefetchContainer::GetEligibilityResultForRedirect(
    const GURL& url) {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  DCHECK(this_prefetch);
  return this_prefetch->is_eligible_;
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
  // Transfer the OnReceivedHeadCallback to the last streaming URL loader.
  if (!streaming_loaders_.empty()) {
    streaming_loader->SetOnReceivedHeadCallback(
        streaming_loaders_.back()->ReleaseOnReceivedHeadCallback());
  }

  streaming_loaders_.push_back(std::move(streaming_loader));
}

PrefetchStreamingURLLoader* PrefetchContainer::GetFirstStreamingURLLoader()
    const {
  if (streaming_loaders_.empty()) {
    return nullptr;
  }
  return streaming_loaders_[0].get();
}

PrefetchStreamingURLLoader* PrefetchContainer::GetLastStreamingURLLoader()
    const {
  if (streaming_loaders_.empty()) {
    return nullptr;
  }
  return streaming_loaders_.back().get();
}

std::unique_ptr<PrefetchStreamingURLLoader>
PrefetchContainer::ReleaseFirstStreamingURLLoader() {
  CHECK(!streaming_loaders_.empty() &&
        streaming_loaders_[0]->IsReadyToServeLastEvents());

  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::move(streaming_loaders_[0]);
  streaming_loaders_.erase(streaming_loaders_.begin());
  return streaming_loader;
}

void PrefetchContainer::ResetAllStreamingURLLoaders() {
  CHECK(!streaming_loaders_.empty());
  for (auto& streaming_loader : streaming_loaders_) {
    DCHECK(streaming_loader);

    // The streaming URL loader can be deleted in one of its callbacks, so
    // instead of deleting it immediately, it is made self owned and then
    // deletes itself.
    PrefetchStreamingURLLoader* raw_streaming_loader = streaming_loader.get();
    raw_streaming_loader->MakeSelfOwnedAndDeleteSoon(
        std::move(streaming_loader));
  }
  streaming_loaders_.clear();
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
      SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
          PrefetchStatus::kPrefetchNotUsedProbeFailed);
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

  if (streaming_loaders_.empty()) {
    return;
  }

  UpdatePrefetchRequestMetrics(
      GetLastStreamingURLLoader()->GetCompletionStatus(),
      GetLastStreamingURLLoader()->GetHead());
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
  if (streaming_loaders_.empty() || GetLastStreamingURLLoader()->GetHead() ||
      GetLastStreamingURLLoader()->Failed()) {
    return false;
  }
  return PrefetchShouldBlockUntilHead(prefetch_type_.GetEagerness());
}

bool PrefetchContainer::IsPrefetchServable(
    base::TimeDelta cacheable_duration) const {
  // Whether or not the response (either full or partial) from the streaming URL
  // loader is servable.
  return !streaming_loaders_.empty() &&
         GetLastStreamingURLLoader()->Servable(cacheable_duration);
}

bool PrefetchContainer::DoesCurrentURLToServeMatch(const GURL& url) const {
  DCHECK(index_redirect_chain_to_serve_ >= 1 &&
         index_redirect_chain_to_serve_ < redirect_chain_.size());
  return IsMatchingURL(redirect_chain_[index_redirect_chain_to_serve_]->url_,
                       url);
}

const GURL& PrefetchContainer::GetCurrentURLToServe() const {
  DCHECK(index_redirect_chain_to_serve_ >= 0 &&
         index_redirect_chain_to_serve_ < redirect_chain_.size());
  return redirect_chain_[index_redirect_chain_to_serve_]->url_;
}

const network::mojom::URLResponseHead* PrefetchContainer::GetHead() {
  PrefetchStreamingURLLoader* streaming_loader = GetLastStreamingURLLoader();
  return streaming_loader ? streaming_loader->GetHead() : nullptr;
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
      GetPrefetchType().IsProxyRequiredWhenCrossOrigin());
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
  // TODO(https://crbug.com/1444568): Handle the case where the given URL
  // matches multiple entries in |redirect_chain_|.
  for (auto itr = redirect_chain_.rbegin(); itr != redirect_chain_.rend();
       itr++) {
    GURL single_prefetch_url = (*itr)->url_;
    if (IsMatchingURL(single_prefetch_url, url)) {
      return itr->get();
    }
  }
  NOTREACHED();
  return nullptr;
}

PrefetchContainer::SinglePrefetch* PrefetchContainer::GetPreviousSinglePrefetch(
    const GURL& url) const {
  // TODO(https://crbug.com/1444568): Handle the case where the given URL
  // matches multiple entries in |redirect_chain_|.
  for (auto itr = redirect_chain_.rbegin(); itr != redirect_chain_.rend();
       itr++) {
    GURL single_prefetch_url = (*itr)->url_;
    if (IsMatchingURL(single_prefetch_url, url)) {
      // Once the SinglePrefetch that matches the given URL is found, then
      // increment the reverse iterator to get the previous one.
      itr++;
      return itr != redirect_chain_.rend() ? itr->get() : nullptr;
    }
  }
  NOTREACHED();
  return nullptr;
}

bool PrefetchContainer::IsMatchingURL(const GURL& internal_url,
                                      const GURL& external_url) const {
  // Check if the URLs match directly.
  if (internal_url == external_url) {
    return true;
  }

  // Otherwise, try to use no_vary_search_helper_.
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
  // OnGetPrefetchToServe is called before we start waiting for head, and
  // when the prefetch is used from `prefetches_ready_to_serve_`.
  // If the prefetch had to wait for head, `blocked_until_head_start_time_`
  // will already be set. Only record in the histogram when the
  // `blocked_until_head_start_time_` is not set yet.
  if (!blocked_until_head_start_time_) {
    RecordWasBlockedUntilHeadWhenServingHistogram(prefetch_type_.GetEagerness(),
                                                  blocked_until_head);
  }
  if (blocked_until_head) {
    blocked_until_head_start_time_ = base::TimeTicks::Now();
  }
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

bool PrefetchContainer::IsIsolatedNetworkContextRequiredForURL(
    const GURL& url) const {
  SinglePrefetch* this_prefetch = GetSinglePrefetch(url);
  CHECK(this_prefetch);
  return this_prefetch->is_isolated_network_context_required_;
}

bool PrefetchContainer::IsIsolatedNetworkContextRequiredForPreviousRedirectHop(
    const GURL& url) const {
  SinglePrefetch* previous_prefetch = GetPreviousSinglePrefetch(url);
  CHECK(previous_prefetch);
  return previous_prefetch->is_isolated_network_context_required_;
}

bool PrefetchContainer::IsProxyRequiredForURL(const GURL& url) const {
  return !referring_origin_.IsSameOriginWith(url) &&
         prefetch_type_.IsProxyRequiredWhenCrossOrigin();
}

std::ostream& operator<<(std::ostream& ostream,
                         const PrefetchContainer& prefetch_container) {
  return ostream << "PrefetchContainer[" << &prefetch_container
                 << ", URL=" << prefetch_container.GetURL() << "]";
}

PrefetchContainer::SinglePrefetch::SinglePrefetch(
    const GURL& url,
    const net::SchemefulSite& referring_site)
    : url_(url) {
  net::SchemefulSite this_site(url_);
  is_isolated_network_context_required_ = referring_site != this_site;
}

PrefetchContainer::SinglePrefetch::~SinglePrefetch() = default;

}  // namespace content
