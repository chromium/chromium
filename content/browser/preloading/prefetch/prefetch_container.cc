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
#include "content/browser/preloading/prefetch/prefetch_features.h"
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
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/redirect_util.h"
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
    case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
      return PreloadingEligibility::kDataSaverEnabled;
    case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
      return PreloadingEligibility::kBatterySaverEnabled;
    case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
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
      case PrefetchStatus::kPrefetchIneligibleBrowserContextOffTheRecord:
      case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
      case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
      case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
      case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
      case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
      case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
      case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
      case PrefetchStatus::kPrefetchIneligibleRetryAfter:
      case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
      case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
      case PrefetchStatus::kPrefetchIneligibleExistingProxy:
      case PrefetchStatus::
          kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
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
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::kPrefetchIneligibleBrowserContextOffTheRecord:
    case PrefetchStatus::
        kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
      return PreloadingTriggeringOutcome::kFailure;
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchAllowed:
    case PrefetchStatus::kPrefetchNotStarted:
    case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
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

  if (old_prefetch_status &&
      new_prefetch_status == PrefetchStatus::kPrefetchEvicted) {
    // Skip this update if the triggering outcome has already been updated to
    // kFailure.
    if (TriggeringOutcomeFromStatus(old_prefetch_status.value()) ==
        PreloadingTriggeringOutcome::kFailure) {
      return;
    }
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
      case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
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
      case PrefetchStatus::kPrefetchIneligibleBrowserContextOffTheRecord:
      case PrefetchStatus::kPrefetchIsStale:
      case PrefetchStatus::kPrefetchNotUsedProbeFailed:
      case PrefetchStatus::
          kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
        NOTIMPLEMENTED();
    }
  }
}

void RecordWasBlockedUntilHeadWhenServingHistogram(
    const blink::mojom::SpeculationEagerness& eagerness,
    bool blocked_until_head) {
  base::UmaHistogramBoolean(
      base::StringPrintf(
          "PrefetchProxy.AfterClick.WasBlockedUntilHeadWhenServing.%s",
          GetPrefetchEagernessHistogramSuffix(eagerness).c_str()),
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
          GetPrefetchEagernessHistogramSuffix(eagerness).c_str()),
      block_until_head_duration);
}

ukm::SourceId GetUkmSourceId(
    base::WeakPtr<PrefetchDocumentManager>& prefetch_document_manager) {
  if (!prefetch_document_manager) {
    return ukm::kInvalidSourceId;
  }
  // Prerendering page should not trigger prefetches.
  CHECK(!prefetch_document_manager->render_frame_host().IsInLifecycleState(
      RenderFrameHost::LifecycleState::kPrerendering));
  return prefetch_document_manager->render_frame_host().GetPageUkmSourceId();
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
                          const net::SchemefulSite& referring_site);
  ~SinglePrefetch();

  SinglePrefetch(const SinglePrefetch&) = delete;
  SinglePrefetch& operator=(const SinglePrefetch&) = delete;

  // The URL that will potentially be prefetched. This can be the original
  // prefetch URL, or a URL from a redirect resulting from requesting the
  // original prefetch URL.
  const GURL url_;

  const bool is_isolated_network_context_required_;

  // Whether this |url_| is eligible to be prefetched
  absl::optional<bool> is_eligible_;

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
  mutable absl::optional<base::TimeTicks> cookie_copy_start_time_;
  mutable absl::optional<base::TimeTicks> cookie_read_end_and_write_start_time_;

  // A callback that runs once |cookie_copy_status_| is set to |kCompleted|.
  mutable base::OnceClosure on_cookie_copy_complete_callback_;
};

PrefetchContainer::PrefetchContainer(
    const GlobalRenderFrameHostId& referring_render_frame_host_id,
    const blink::DocumentToken& referring_document_token,
    const GURL& url,
    const PrefetchType& prefetch_type,
    const blink::mojom::Referrer& referrer,
    absl::optional<net::HttpNoVarySearchData> no_vary_search_hint,
    blink::mojom::SpeculationInjectionWorld world,
    base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager)
    : referring_render_frame_host_id_(referring_render_frame_host_id),
      referring_document_token_(referring_document_token),
      prefetch_url_(url),
      prefetch_type_(prefetch_type),
      referrer_(referrer),
      referring_origin_(url::Origin::Create(referrer_.url)),
      referring_site_(net::SchemefulSite(referrer_.url)),
      no_vary_search_hint_(std::move(no_vary_search_hint)),
      prefetch_document_manager_(prefetch_document_manager),
      ukm_source_id_(GetUkmSourceId(prefetch_document_manager_)),
      request_id_(base::UnguessableToken::Create().ToString()) {
  auto* rfhi = RenderFrameHostImpl::FromID(referring_render_frame_host_id);
  // Note: |rfhi| is only nullptr in unit tests.
  if (rfhi) {
    auto* web_contents = WebContents::FromRenderFrameHost(rfhi);
    auto* preloading_data =
        PreloadingData::GetOrCreateForWebContents(web_contents);
    auto matcher =
        base::FeatureList::IsEnabled(network::features::kPrefetchNoVarySearch)
            ? PreloadingDataImpl::GetSameURLAndNoVarySearchURLMatcher(
                  prefetch_document_manager_, prefetch_url_)
            : PreloadingDataImpl::GetSameURLMatcher(prefetch_url_);
    auto* attempt = static_cast<PreloadingAttemptImpl*>(
        preloading_data->AddPreloadingAttempt(
            GetPredictorForSpeculationRules(world), PreloadingType::kPrefetch,
            std::move(matcher),
            web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()));
    attempt->SetSpeculationEagerness(prefetch_type.GetEagerness());
    attempt_ = attempt->GetWeakPtr();
    initiator_devtools_navigation_token_ = rfhi->GetDevToolsNavigationToken();
  }

  // `PreloadingPrediction` is added in `PreloadingDecider`.

  redirect_chain_.push_back(
      std::make_unique<SinglePrefetch>(prefetch_url_, referring_site_));
}

PrefetchContainer::~PrefetchContainer() {
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

  // TODO(https://crbug.com/1299059): Get the navigation start time and set the
  // NavigationStartToFetchStartMs field of the PrefetchProxy.PrefetchedResource
  // UKM event.

  builder.Record(ukm::UkmRecorder::Get());

  if (prefetch_document_manager_) {
    prefetch_document_manager_->PrefetchWillBeDestroyed(this);
  }
}

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
  FrameTreeNode* ftn = FrameTreeNode::From(
      RenderFrameHostImpl::FromID(referring_render_frame_host_id_));

  absl::optional<PreloadingTriggeringOutcome> preloading_trigger_outcome =
      TriggeringOutcomeFromStatus(prefetch_status);

  if (initiator_devtools_navigation_token_.has_value() &&
      preloading_trigger_outcome.has_value()) {
    devtools_instrumentation::DidUpdatePrefetchStatus(
        ftn, initiator_devtools_navigation_token_.value(), prefetch_url_,
        preloading_trigger_outcome.value(), prefetch_status, RequestId());
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

PrefetchNetworkContext*
PrefetchContainer::GetOrCreateNetworkContextForCurrentPrefetch(
    PrefetchService* prefetch_service) {
  bool is_isolated_network_context_required =
      IsIsolatedNetworkContextRequiredForCurrentPrefetch();

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

void PrefetchContainer::OnEligibilityCheckComplete(
    bool is_eligible,
    absl::optional<PrefetchStatus> status) {
  SinglePrefetch& this_prefetch = GetCurrentSinglePrefetchToPrefetch();
  this_prefetch.is_eligible_ = is_eligible;

  if (redirect_chain_.size() == 1) {
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

void PrefetchContainer::AddRedirectHop(const net::RedirectInfo& redirect_info) {
  CHECK(resource_request_);
  CHECK(base::FeatureList::IsEnabled(features::kPrefetchRedirects));

  // There are sometimes other headers that are modified during navigation
  // redirects; see |NavigationRequest::OnRedirectChecksComplete| (including
  // some which are added by throttles). These aren't yet supported for
  // prefetch, including browsing topics and client hints.
  net::HttpRequestHeaders updated_headers;
  updated_headers.SetHeader("Sec-Purpose",
                            IsProxyRequiredForURL(redirect_info.new_url)
                                ? "prefetch;anonymous-client-ip"
                                : "prefetch");

  // TODO(jbroman): We have several places that invoke
  // `net::RedirectUtil::UpdateHttpRequest` and then need to do very similar
  // work afterward. Ideally we would deduplicate these more.
  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      resource_request_->url, resource_request_->method, redirect_info,
      /*removed_headers=*/absl::nullopt, std::move(updated_headers),
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

  redirect_chain_.push_back(
      std::make_unique<SinglePrefetch>(redirect_info.new_url, referring_site_));
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

PrefetchRequestHandler PrefetchContainer::Reader::CreateRequestHandler() {
  // Create a `PrefetchRequestHandler` from the current `SinglePrefetch` (==
  // `reader`) and its corresponding `PrefetchStreamingURLLoader`.
  auto handler = GetCurrentSinglePrefetchToServe()
                     .response_reader_->CreateRequestHandler();

  // Advance the current `SinglePrefetch` position.
  AdvanceCurrentURLToServe();

  return handler;
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
      prefetch_container_->SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
          PrefetchStatus::kPrefetchNotUsedProbeFailed);
      break;
    default:
      NOTIMPLEMENTED();
  }
}

void PrefetchContainer::SetNoVarySearchData(RenderFrameHost* rfh) {
  CHECK(!no_vary_search_data_);
  // Check `GetHead()` here, because `OnReceivedHead()` can be called in
  // non-servable cases when response headers are not available.
  if (!GetHead()) {
    return;
  }
  no_vary_search_data_ =
      no_vary_search::ProcessHead(*GetHead(), prefetch_url_, rfh);
}

void PrefetchContainer::OnReceivedHead() {
  if (prefetch_document_manager_ &&
      prefetch_document_manager_->NoVarySearchSupportEnabled()) {
    SetNoVarySearchData(&prefetch_document_manager_->render_frame_host());
  }

  if (on_received_head_callback_) {
    std::move(on_received_head_callback_).Run();
  }
}

void PrefetchContainer::SetOnReceivedHeadCallback(
    base::OnceClosure on_received_head_callback) {
  on_received_head_callback_ = std::move(on_received_head_callback);
}

base::OnceClosure PrefetchContainer::ReleaseOnReceivedHeadCallback() {
  return std::move(on_received_head_callback_);
}

void PrefetchContainer::StartTimeoutTimer(
    base::TimeDelta timeout,
    base::OnceClosure on_timeout_callback) {
  CHECK(!timeout_timer_);
  timeout_timer_ = std::make_unique<base::OneShotTimer>();
  timeout_timer_->Start(FROM_HERE, timeout, std::move(on_timeout_callback));
}

void PrefetchContainer::OnPrefetchComplete() {
  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.Prefetch.RedirectChainSize",
                           redirect_chain_.size());
  DVLOG(1) << *this << "::OnPrefetchComplete";
  if (!GetNonRedirectResponseReader()) {
    DVLOG(1) << *this << "::OnPrefetchComplete:"
             << "no non redirect response reader";
    return;
  }

  UpdatePrefetchRequestMetrics(
      GetNonRedirectResponseReader()->GetCompletionStatus(),
      GetNonRedirectResponseReader()->GetHead());
  UpdateServingPageMetrics();
}

void PrefetchContainer::UpdatePrefetchRequestMetrics(
    const absl::optional<network::URLLoaderCompletionStatus>& completion_status,
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

void PrefetchContainer::TakeBlockUntilHeadTimer(
    std::unique_ptr<base::OneShotTimer> block_until_head_timer) {
  block_until_head_timer_ = std::move(block_until_head_timer);
}

void PrefetchContainer::ResetBlockUntilHeadTimer() {
  if (block_until_head_timer_) {
    block_until_head_timer_->AbandonAndStop();
  }
  block_until_head_timer_.reset();
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
      redirect_chain_.back()->response_reader_->IsWaitingForResponse() &&
      PrefetchShouldBlockUntilHead(prefetch_type_.GetEagerness())) {
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

const network::mojom::URLResponseHead* PrefetchContainer::GetHead() {
  return GetNonRedirectResponseReader()
             ? GetNonRedirectResponseReader()->GetHead()
             : nullptr;
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

void PrefetchContainer::SimulateAttemptAtInterceptorForTest() {
  if (attempt_) {
    attempt_->SetEligibility(PreloadingEligibility::kEligible);
    attempt_->SetHoldbackStatus(PreloadingHoldbackStatus::kAllowed);
  }
  SetPrefetchStatus(PrefetchStatus::kPrefetchAllowed);
  SetPrefetchStatus(PrefetchStatus::kPrefetchSuccessful);
}

void PrefetchContainer::OnCookiesChanged() {
  SetPrefetchStatus(PrefetchStatus::kPrefetchNotUsedCookiesChanged);
  UpdateServingPageMetrics();
  CancelStreamingURLLoaderIfNotServing();
}

// TODO(crbug.com/1462206): We might be waiting on PrefetchContainer's head
// from multiple navigations.
// E.g. We might wait from one navigation but not use the prefetch, and
// then we can use the prefetch in a separate navigation without waiting
// for the head. We need to keep track of blocked_until_head_start_time_ per
// each navigation for this PrefetchContainer.
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

GURL PrefetchContainer::GetCurrentURL() const {
  return GetCurrentSinglePrefetchToPrefetch().url_;
}

GURL PrefetchContainer::GetPreviousURL() const {
  return GetPreviousSinglePrefetchToPrefetch().url_;
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
  request->referrer = GetReferrer().url;
  request->referrer_policy =
      Referrer::ReferrerPolicyForUrlRequest(GetReferrer().policy);
  request->enable_load_timing = true;
  // TODO(https://crbug.com/1317756): Investigate if we need to include the
  // net::LOAD_DISABLE_CACHE flag.
  request->load_flags = net::LOAD_DISABLE_CACHE | net::LOAD_PREFETCH;
  request->credentials_mode = network::mojom::CredentialsMode::kInclude;
  request->headers.MergeFrom(additional_headers);
  request->headers.SetHeader(kCorsExemptPurposeHeaderName, "prefetch");
  request->headers.SetHeader("Sec-Purpose", IsProxyRequiredForURL(url)
                                                ? "prefetch;anonymous-client-ip"
                                                : "prefetch");
  request->headers.SetHeader("Upgrade-Insecure-Requests", "1");

  // Remove the user agent header if it was set so that the network context's
  // default is used.
  request->headers.RemoveHeader("User-Agent");

  // There are sometimes other headers that are set during navigation.  These
  // aren't yet supported for prefetch, including browsing topics and client
  // hints.

  request->trusted_params = trusted_params;
  request->site_for_cookies = trusted_params.isolation_info.site_for_cookies();

  // This causes us to reset the site for cookies on cross-site redirect. This
  // is correct as long as we are looking at top-level navigations. If we ever
  // implement prefetching for subframes, this will need to consider that.
  // See also the code which sets this in |NavigationUrlLoaderImpl|.
  request->update_first_party_url_on_redirect = true;

  request->devtools_request_id = RequestId();

  // This may seem inverted (surely eager prefetches would be higher priority),
  // but the fact that we're doing this at all for more conservative candidates
  // suggests a strong engagement signal.
  //
  // TODO(crbug.com/1467928): Ideally, we would actually use a combination of
  // the actual engagement seen (rather than the minimum required to trigger the
  // candidate) and the declared eagerness, and update them as the prefetch
  // becomes increasingly likely.
  blink::mojom::SpeculationEagerness eagerness =
      GetPrefetchType().GetEagerness();
  switch (eagerness) {
    case blink::mojom::SpeculationEagerness::kConservative:
      request->priority = net::RequestPriority::MEDIUM;
      break;
    case blink::mojom::SpeculationEagerness::kModerate:
      request->priority = net::RequestPriority::LOW;
      break;
    case blink::mojom::SpeculationEagerness::kEager:
      request->priority = net::RequestPriority::IDLE;
      break;
  }

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

std::ostream& operator<<(std::ostream& ostream,
                         const PrefetchContainer& prefetch_container) {
  return ostream << "PrefetchContainer[" << &prefetch_container
                 << ", Key=" << prefetch_container.GetPrefetchContainerKey()
                 << "]";
}

std::ostream& operator<<(std::ostream& ostream,
                         const PrefetchContainer::Key& prefetch_key) {
  return ostream << "(" << prefetch_key.first << ", " << prefetch_key.second
                 << ")";
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
    const net::SchemefulSite& referring_site)
    : url_(url),
      is_isolated_network_context_required_(referring_site !=
                                            net::SchemefulSite(url_)),
      response_reader_(base::MakeRefCounted<PrefetchResponseReader>()) {}

PrefetchContainer::SinglePrefetch::~SinglePrefetch() {
  CHECK(response_reader_);
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(response_reader_));
}

}  // namespace content
