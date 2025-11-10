// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_service.h"

#include <optional>
#include <string_view>

#include "base/auto_reset.h"
#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_handle_impl.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_proxy_configurator.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_scheduler.h"
#include "content/browser/preloading/prefetch/prefetch_servable_state.h"
#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/proxy_lookup_client_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

static ServiceWorkerContext* g_service_worker_context_for_testing = nullptr;

bool (*g_host_non_unique_filter)(std::string_view) = nullptr;

static network::SharedURLLoaderFactory* g_url_loader_factory_for_testing =
    nullptr;

static network::mojom::NetworkContext*
    g_network_context_for_proxy_lookup_for_testing = nullptr;

PrefetchService::InjectedEligibilityCheckForTesting&
GetInjectedEligibilityCheckForTesting() {
  static base::NoDestructor<PrefetchService::InjectedEligibilityCheckForTesting>
      prefetch_injected_eligibility_check_for_testing;
  return *prefetch_injected_eligibility_check_for_testing;
}

bool ShouldConsiderDecoyRequestForStatus(PreloadingEligibility eligibility) {
  switch (eligibility) {
    case PreloadingEligibility::kUserHasCookies:
    case PreloadingEligibility::kUserHasServiceWorker:
    case PreloadingEligibility::kUserHasServiceWorkerNoFetchHandler:
    case PreloadingEligibility::kRedirectFromServiceWorker:
    case PreloadingEligibility::kRedirectToServiceWorker:
      // If the prefetch is not eligible because of cookie or a service worker,
      // then maybe send a decoy.
      return true;
    case PreloadingEligibility::kBatterySaverEnabled:
    case PreloadingEligibility::kDataSaverEnabled:
    case PreloadingEligibility::kExistingProxy:
    case PreloadingEligibility::kHostIsNonUnique:
    case PreloadingEligibility::kNonDefaultStoragePartition:
    case PreloadingEligibility::kPrefetchProxyNotAvailable:
    case PreloadingEligibility::kPreloadingDisabled:
    case PreloadingEligibility::kRetryAfter:
    case PreloadingEligibility::kSameSiteCrossOriginPrefetchRequiredProxy:
    case PreloadingEligibility::kSchemeIsNotHttps:
      // These statuses don't relate to any user state, so don't send a decoy
      // request.
      return false;
    case PreloadingEligibility::kEligible:
    default:
      // Other ineligible cases are not used in `PrefetchService`.
      NOTREACHED();
  }
}

bool ShouldStartSpareRenderer() {
  if (!PrefetchStartsSpareRenderer()) {
    return false;
  }

  for (RenderProcessHost::iterator iter(RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrentValue()->IsUnused()) {
      // There is already a spare renderer.
      return false;
    }
  }
  return true;
}

void RecordPrefetchProxyPrefetchMainframeTotalTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::Time start = head->request_time;
  base::Time end = head->response_time;

  if (start.is_null() || end.is_null()) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES("PrefetchProxy.Prefetch.Mainframe.TotalTime",
                             end - start, base::Milliseconds(10),
                             base::Seconds(30), 100);
}

void RecordPrefetchProxyPrefetchMainframeConnectTime(
    network::mojom::URLResponseHead* head) {
  DCHECK(head);

  base::TimeTicks start = head->load_timing.connect_timing.connect_start;
  base::TimeTicks end = head->load_timing.connect_timing.connect_end;

  if (start.is_null() || end.is_null()) {
    return;
  }

  UMA_HISTOGRAM_TIMES("PrefetchProxy.Prefetch.Mainframe.ConnectTime",
                      end - start);
}

void RecordPrefetchProxyPrefetchMainframeRespCode(int response_code) {
  base::UmaHistogramSparse("PrefetchProxy.Prefetch.Mainframe.RespCode",
                           response_code);
}

void RecordPrefetchProxyPrefetchMainframeCookiesToCopy(
    size_t cookie_list_size) {
  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.Prefetch.Mainframe.CookiesToCopy",
                           cookie_list_size);
}

void CookieSetHelper(base::RepeatingClosure closure,
                     net::CookieAccessResult access_result) {
  closure.Run();
}

// Returns true if the prefetch is heldback, and set the holdback status
// correspondingly.
bool CheckAndSetPrefetchHoldbackStatus(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  if (!prefetch_container->request().attempt()) {
    return false;
  }

  bool devtools_client_exist = [&] {
    // Currently DevTools only supports when the prefetch is initiated by
    // renderer.
    auto* renderer_initiator_info =
        prefetch_container->request().GetRendererInitiatorInfo();
    if (!renderer_initiator_info) {
      return false;
    }
    RenderFrameHostImpl* initiator_rfh =
        renderer_initiator_info->GetRenderFrameHost();
    return initiator_rfh &&
           RenderFrameDevToolsAgentHost::GetFor(initiator_rfh) != nullptr;
  }();

  // Normally PreloadingAttemptImpl::ShouldHoldback() eventually computes its
  // `holdback_status_`, but we forcely set the status in some special cases
  // below, by calling PreloadingAttemptImpl::SetHoldbackStatus().
  // As its comment describes, this is expected to be called only once.
  //
  // Note that, alternatively, determining holdback status can be done in
  // triggers, e.g. in `PreloadingAttemptImpl::ctor()`. For more details, see
  // https://crbug.com/406123867

  if (devtools_client_exist) {
    // 1. When developers debug Speculation Rules Prefetch using DevTools,
    // always set status to kAllowed for developer experience.
    prefetch_container->request().attempt()->SetHoldbackStatus(
        PreloadingHoldbackStatus::kAllowed);
  } else if (prefetch_container->IsLikelyAheadOfPrerender()) {
    // 2. If PrefetchContainer is likely ahead of prerender, always set status
    // to kAllowed as it is likely used for prerender.
    //
    // Note that we don't use
    // `PrefetchContainer::request().holdback_status_override()` for this
    // purpose because it can't handle a prefetch that was not ahead of
    // prerender but another ahead of prerender one is migrated into it. We need
    // to update migration if we'd like to do it.
    prefetch_container->request().attempt()->SetHoldbackStatus(
        PreloadingHoldbackStatus::kAllowed);
  } else if (prefetch_container->request().holdback_status_override() !=
             PreloadingHoldbackStatus::kUnspecified) {
    // 3. If PrefetchContainer has custom overridden status, set that value.
    prefetch_container->request().attempt()->SetHoldbackStatus(
        prefetch_container->request().holdback_status_override());
  }

  if (prefetch_container->request().attempt()->ShouldHoldback()) {
    prefetch_container->SetLoadState(
        PrefetchContainer::LoadState::kFailedHeldback);
    prefetch_container->SetPrefetchStatus(PrefetchStatus::kPrefetchHeldback);
    return true;
  }
  return false;
}

BrowserContext* BrowserContextFromFrameTreeNodeId(
    FrameTreeNodeId frame_tree_node_id) {
  WebContents* web_content =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_content) {
    return nullptr;
  }
  return web_content->GetBrowserContext();
}

void RecordRedirectResult(PrefetchRedirectResult result) {
  UMA_HISTOGRAM_ENUMERATION("PrefetchProxy.Redirect.Result", result);
}

void RecordRedirectNetworkContextTransition(
    bool previous_requires_isolated_network_context,
    bool redirect_requires_isolated_network_context) {
  PrefetchRedirectNetworkContextTransition transition;
  if (!previous_requires_isolated_network_context &&
      !redirect_requires_isolated_network_context) {
    transition = PrefetchRedirectNetworkContextTransition::kDefaultToDefault;
  }
  if (!previous_requires_isolated_network_context &&
      redirect_requires_isolated_network_context) {
    transition = PrefetchRedirectNetworkContextTransition::kDefaultToIsolated;
  }
  if (previous_requires_isolated_network_context &&
      !redirect_requires_isolated_network_context) {
    transition = PrefetchRedirectNetworkContextTransition::kIsolatedToDefault;
  }
  if (previous_requires_isolated_network_context &&
      redirect_requires_isolated_network_context) {
    transition = PrefetchRedirectNetworkContextTransition::kIsolatedToIsolated;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "PrefetchProxy.Redirect.NetworkContextStateTransition", transition);
}

void OnIsolatedCookieCopyComplete(PrefetchServingHandle serving_handle) {
  if (serving_handle) {
    serving_handle.OnIsolatedCookieCopyComplete();
  }
}

bool IsReferrerPolicySufficientlyStrict(
    const network::mojom::ReferrerPolicy& referrer_policy) {
  // https://github.com/WICG/nav-speculation/blob/main/prefetch.bs#L606
  // "", "`strict-origin-when-cross-origin`", "`strict-origin`",
  // "`same-origin`", "`no-referrer`".
  switch (referrer_policy) {
    case network::mojom::ReferrerPolicy::kDefault:
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
    case network::mojom::ReferrerPolicy::kSameOrigin:
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      return true;
    case network::mojom::ReferrerPolicy::kAlways:
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
    case network::mojom::ReferrerPolicy::kNever:
    case network::mojom::ReferrerPolicy::kOrigin:
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      return false;
  }
}

}  // namespace

// static
PrefetchService* PrefetchService::GetFromFrameTreeNodeId(
    FrameTreeNodeId frame_tree_node_id) {
  BrowserContext* browser_context =
      BrowserContextFromFrameTreeNodeId(frame_tree_node_id);
  if (!browser_context) {
    return nullptr;
  }
  return BrowserContextImpl::From(browser_context)->GetPrefetchService();
}

void PrefetchService::SetFromFrameTreeNodeIdForTesting(
    FrameTreeNodeId frame_tree_node_id,
    std::unique_ptr<PrefetchService> prefetch_service) {
  BrowserContext* browser_context =
      BrowserContextFromFrameTreeNodeId(frame_tree_node_id);
  CHECK(browser_context);
  return BrowserContextImpl::From(browser_context)
      ->SetPrefetchServiceForTesting(std::move(prefetch_service));  // IN-TEST
}

PrefetchService::PrefetchService(BrowserContext* browser_context)
    : browser_context_(browser_context),
      delegate_(GetContentClient()->browser()->CreatePrefetchServiceDelegate(
          browser_context_)),
      prefetch_proxy_configurator_(
          PrefetchProxyConfigurator::MaybeCreatePrefetchProxyConfigurator(
              PrefetchProxyHost(delegate_
                                    ? delegate_->GetDefaultPrefetchProxyHost()
                                    : GURL("")),
              delegate_ ? delegate_->GetAPIKey() : "")),
      origin_prober_(std::make_unique<PrefetchOriginProber>(
          browser_context_,
          PrefetchDNSCanaryCheckURL(
              delegate_ ? delegate_->GetDefaultDNSCanaryCheckURL() : GURL("")),
          PrefetchTLSCanaryCheckURL(
              delegate_ ? delegate_->GetDefaultTLSCanaryCheckURL()
                        : GURL("")))),
      scheduler_(UsePrefetchScheduler()
                     ? std::make_unique<PrefetchScheduler>(this)
                     : nullptr) {}

PrefetchService::~PrefetchService() {  // NOLINT(modernize-use-equals-default)
  // This is to avoid notifying `PrefetchService::OnWillBeDestroyed()` in the
  // middle of `PrefetchService` destruction.
  // This is for the future when we add some logic to `OnWillBeDestroyed()`.
  // Currently `OnWillBeDestroyed()` is empty and thus this is no-op and we have
  // to skip `modernize-use-equals-default` check.
  for (const auto& iter : owned_prefetches()) {
    if (iter.second) {
      iter.second->RemoveObserver(this);
    }
  }
}

void PrefetchService::SetPrefetchServiceDelegateForTesting(
    std::unique_ptr<PrefetchServiceDelegate> delegate) {
  DCHECK(!delegate_);
  delegate_ = std::move(delegate);
}

PrefetchOriginProber* PrefetchService::GetPrefetchOriginProber() const {
  return origin_prober_.get();
}

base::WeakPtr<PrefetchContainer> PrefetchService::AddPrefetchRequestInternal(
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  enum class Action {
    kTakeOldWithMigration,
    kReplaceOldWithNew,
    kTakeNew,
  };

  // The comment below might be old. Currently,
  // `PrefetchDocumentManager::PrefetchUrl()` reject colficting prefetches
  // execpt for ahead of prerender case.
  //
  // TODO(crbug.com/371179869): Integrate these two processes.
  //
  // A newly submitted prefetch could already be in |owned_prefetches_| if and
  // only if:
  //   1) There was a same origin navigaition that used the same renderer.
  //   2) Both pages requested a prefetch for the same URL.
  //   3) The prefetch from the first page had at least started its network
  //      request (which would mean that it is in |owned_prefetches_| and owned
  //      by the prefetch service).
  // If this happens, then we just delete the old prefetch and add the new
  // prefetch to |owned_prefetches_|.
  //
  // Note that we might replace this by preserving existing prefetch and
  // additional works, e.g. adding some properties to the old one and prolonging
  // cacheable duration, to prevent additional fetch. See also
  // https://chromium-review.googlesource.com/c/chromium/src/+/3880874/comment/5ecccbf7_8fbcba96/
  //
  // TODO(crbug.com/372186548): Revisit the merging process and comments here
  // and below.
  auto prefetch_iter = owned_prefetches().find(prefetch_request->key());
  Action action = [&]() {
    if (prefetch_iter == owned_prefetches().end()) {
      return Action::kTakeNew;
    }
    PrefetchContainer& prefetch_container_old = *prefetch_iter->second;

    if (!features::UsePrefetchPrerenderIntegration()) {
      return Action::kReplaceOldWithNew;
    }

    switch (
        prefetch_container_old.GetServableState(PrefetchCacheableDuration())) {
      case PrefetchServableState::kNotServable:
        return Action::kReplaceOldWithNew;
      case PrefetchServableState::kShouldBlockUntilEligibilityGot:
      case PrefetchServableState::kShouldBlockUntilHeadReceived:
      case PrefetchServableState::kServable:
        // nop
        break;
    }

    // Take preload pipeline info of prefetch ahead of prerender.
    //
    // Consider the following screnario (especially, in the same SpecRules and
    // upgrading ):
    //
    // - A document adds SpecRules of prefetch A for URL X.
    // - A document adds SpecRules of prerender B' for URL X.
    //
    // With `kPrerender2FallbackPrefetchSpecRules`, B' triggers prefetch ahead
    // of prerender B for URL X. Sites use SpecRules A+B' with expectation
    // "prefetch X then prerender X", but the order of
    // `PrefetchService::AddPrefetchRequest*()` for A and B is unstable in
    // general.
    //
    // `PrerenderHost` of B' needs to know eligibility and status of B. We use
    // `PreloadPipelineInfo` for this purpose.
    //
    // - If A is followed by B, take A and migrate B into A. A inherits
    //   `PreloadPipelineInfo` of B.
    // - If B is followed by A, just reject A (by
    //   `PrefetchDocumentManager::PrefetchUrl()`).
    //
    // See also tests `PrerendererImplBrowserTestPrefetchAhead.*`.

    return Action::kTakeOldWithMigration;
  }();

  switch (action) {
    case Action::kTakeOldWithMigration:
      prefetch_iter->second->MergeNewPrefetchRequest(
          std::move(prefetch_request));
      if (UsePrefetchScheduler()) {
        scheduler_->NotifyAttributeMightChangedAndProgressAsync(
            *prefetch_iter->second, /*should_progress=*/false);
      }
      return nullptr;
    case Action::kReplaceOldWithNew:
      ResetPrefetchContainer(prefetch_iter->second->GetWeakPtr(),
                             /*should_progress=*/false);
      return CreatePrefetchContainer(std::move(prefetch_request));
    case Action::kTakeNew:
      return CreatePrefetchContainer(std::move(prefetch_request));
  }
}

base::WeakPtr<PrefetchContainer> PrefetchService::CreatePrefetchContainer(
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  auto owned_prefetch_container = PrefetchContainer::Create(
      base::PassKey<PrefetchService>(), std::move(prefetch_request));
  const base::WeakPtr<PrefetchContainer> prefetch_container =
      owned_prefetch_container->GetWeakPtr();

  // There should be no existing entry for `prefetch_container_key`.
  CHECK(owned_prefetches_
            .emplace(prefetch_container->key(),
                     std::move(owned_prefetch_container))
            .second);

  prefetch_container->OnAddedToPrefetchService();

  prefetch_container->AddObserver(this);

  return prefetch_container;
}

bool PrefetchService::IsPrefetchDuplicate(
    GURL& url,
    std::optional<net::HttpNoVarySearchData> no_vary_search_hint) {
  TRACE_EVENT("loading", "PrefetchService::IsPrefetchDuplicate");
  for (const auto& [key, prefetch_container] : owned_prefetches()) {
    if (IsPrefetchStale(prefetch_container->GetWeakPtr())) {
      continue;
    }

    // We will only compare the URLs if the no-vary-search hints match for
    // determinism. This is because comparing URLs with different no-vary-search
    // hints will change the outcome of the comparison based on the order the
    // requests happened in.
    //
    // This approach optimizes for determinism over minimizing wasted
    // or redundant prefetches.
    bool nvs_hints_match = no_vary_search_hint ==
                           prefetch_container->request().no_vary_search_hint();
    if (!nvs_hints_match) {
      continue;
    }

    bool urls_equal;
    if (no_vary_search_hint) {
      urls_equal = no_vary_search_hint->AreEquivalent(url, key.url());
    } else {
      // If there is no no-vary-search hint, just compare the URLs.
      urls_equal = url == key.url();
    }

    if (!urls_equal) {
      continue;
    }
    return true;
  }
  return false;
}

bool PrefetchService::IsPrefetchAttemptFailedOrDiscardedInternal(
    base::PassKey<PrefetchDocumentManager>,
    PrefetchKey key) const {
  auto it = owned_prefetches().find(key);
  if (it == owned_prefetches().end() || !it->second) {
    return true;
  }

  const std::unique_ptr<PrefetchContainer>& container = it->second;
  if (!container->HasPrefetchStatus()) {
    return false;  // the container is not processed yet
  }

  switch (container->GetPrefetchStatus()) {
    case PrefetchStatus::kPrefetchSuccessful:
    case PrefetchStatus::kPrefetchResponseUsed:
      return false;
    case PrefetchStatus::kPrefetchIneligibleUserHasCookies:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleUserHasServiceWorkerNoFetchHandler:
    case PrefetchStatus::kPrefetchIneligibleRedirectFromServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleRedirectToServiceWorker:
    case PrefetchStatus::kPrefetchIneligibleSchemeIsNotHttps:
    case PrefetchStatus::kPrefetchIneligibleNonDefaultStoragePartition:
    case PrefetchStatus::kPrefetchIneligibleRetryAfter:
    case PrefetchStatus::kPrefetchIneligiblePrefetchProxyNotAvailable:
    case PrefetchStatus::kPrefetchIneligibleHostIsNonUnique:
    case PrefetchStatus::kPrefetchIneligibleDataSaverEnabled:
    case PrefetchStatus::kPrefetchIneligibleBatterySaverEnabled:
    case PrefetchStatus::kPrefetchIneligiblePreloadingDisabled:
    case PrefetchStatus::kPrefetchIneligibleExistingProxy:
    case PrefetchStatus::kPrefetchIsStale:
    case PrefetchStatus::kPrefetchNotUsedProbeFailed:
    case PrefetchStatus::kPrefetchNotStarted:
    case PrefetchStatus::kPrefetchNotFinishedInTime:
    case PrefetchStatus::kPrefetchFailedNetError:
    case PrefetchStatus::kPrefetchFailedNon2XX:
    case PrefetchStatus::kPrefetchFailedMIMENotSupported:
    case PrefetchStatus::kPrefetchIsPrivacyDecoy:
    case PrefetchStatus::kPrefetchNotUsedCookiesChanged:
    case PrefetchStatus::kPrefetchHeldback:
    case PrefetchStatus::kPrefetchFailedInvalidRedirect:
    case PrefetchStatus::kPrefetchFailedIneligibleRedirect:
    case PrefetchStatus::
        kPrefetchIneligibleSameSiteCrossOriginPrefetchRequiredProxy:
    case PrefetchStatus::kPrefetchEvictedAfterCandidateRemoved:
    case PrefetchStatus::kPrefetchEvictedForNewerPrefetch:
    case PrefetchStatus::kPrefetchEvictedAfterBrowsingDataRemoved:
      return true;
  }
}

bool PrefetchService::IsPrefetchStale(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  TRACE_EVENT("loading", "PrefetchService::IsPrefetchStale");
  if (!prefetch_container) {
    return true;
  }

  // `PrefetchContainer::LoadState` check.
  switch (prefetch_container->GetLoadState()) {
    case PrefetchContainer::LoadState::kFailedIneligible:
    case PrefetchContainer::LoadState::kFailedHeldback:
      return true;
    case PrefetchContainer::LoadState::kNotStarted:
    case PrefetchContainer::LoadState::kEligible:
    case PrefetchContainer::LoadState::kStarted:
    case PrefetchContainer::LoadState::kDeterminedHead:
    case PrefetchContainer::LoadState::kFailedDeterminedHead:
    case PrefetchContainer::LoadState::kCompleted:
    case PrefetchContainer::LoadState::kFailed:
      break;
  }

  // `PrefetchServableState` check.
  PrefetchServableState servable_state =
      prefetch_container->GetServableState(PrefetchCacheableDuration());
  if (servable_state == PrefetchServableState::kNotServable) {
    return true;
  }
  return false;
}

// Parameter class used during eligibility check and `OnGotEligibility*` methods
// (`callback`).
struct PrefetchService::CheckEligibilityParams final {
  void Finish(PreloadingEligibility eligibility) && {
    // `callback_local` is needed to avoid use-after-move.
    auto callback_local = std::move(callback);
    std::move(callback_local).Run(std::move(*this), eligibility);
  }

  // Returns if proxy is required for the next request.
  bool IsProxyRequired() const {
    CHECK(prefetch_container);
    return prefetch_container->IsProxyRequiredForURL(url) &&
           !ShouldPrefetchBypassProxyForTestHost(url.GetHost());
  }

  base::WeakPtr<PrefetchContainer> prefetch_container;

  // The URL of the next request.
  GURL url;

  // Whether this is eligibility check for a redirect, or for an initial
  // request.
  bool is_redirect;

  // TODO(crbug.com/432783906): Add a `CHECK()` to ensure `callback` is always
  // called. However, there are some cases where `callback` is not called (e.g.
  // when `PrefetchService` is destroyed during eligibility check, which a valid
  // exception, as well as other suspicious cases).
  base::OnceCallback<void(CheckEligibilityParams, PreloadingEligibility)>
      callback;
};

std::unique_ptr<PrefetchHandle> PrefetchService::AddPrefetchRequestWithHandle(
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  base::WeakPtr<PrefetchContainer> prefetch_container =
      AddPrefetchRequestInternal(std::move(prefetch_request));

  if (prefetch_container) {
    PrefetchUrl(prefetch_container);
  }

  return std::make_unique<PrefetchHandleImpl>(GetWeakPtr(), prefetch_container);
}

base::WeakPtr<PrefetchContainer>
PrefetchService::AddPrefetchRequestWithoutStartingPrefetchForTesting(
    std::unique_ptr<const PrefetchRequest> prefetch_request) {
  return AddPrefetchRequestInternal(std::move(prefetch_request));
}

void PrefetchService::PrefetchUrl(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  CHECK(prefetch_container);
  TRACE_EVENT("loading", "PrefetchService::PrefetchUrl", "prefetch_url",
              prefetch_container->GetURL());

  auto params = CheckEligibilityParams(
      {.prefetch_container = prefetch_container,
       .url = prefetch_container->GetURL(),
       .is_redirect = false,
       .callback =
           base::BindOnce(&PrefetchService::OnGotEligibilityForNonRedirect,
                          weak_method_factory_.GetWeakPtr())});

  if (delegate_) {
    const auto eligibility_from_delegate = delegate_->IsSomePreloadingEnabled();
    // If pre* actions are disabled then don't prefetch.
    if (eligibility_from_delegate != PreloadingEligibility::kEligible) {
      std::move(params).Finish(eligibility_from_delegate);
      return;
    }

    const auto& prefetch_type = prefetch_container->request().prefetch_type();
    if (prefetch_type.IsProxyRequiredWhenCrossOrigin()) {
      bool allow_all_domains =
          PrefetchAllowAllDomains() ||
          (PrefetchAllowAllDomainsForExtendedPreloading() &&
           delegate_->IsExtendedPreloadingEnabled());
      if (!allow_all_domains &&
          prefetch_container->request().referring_origin().has_value() &&
          !delegate_->IsDomainInPrefetchAllowList(prefetch_container->request()
                                                      .referring_origin()
                                                      .value()
                                                      .GetURL())) {
        DVLOG(1) << *prefetch_container
                 << ": not prefetched (not in allow list)";
        return;
      }
    }

    // TODO(crbug.com/40946257): Current code doesn't support PageLoadMetrics
    // when the prefetch is initiated by browser.
    if (auto* renderer_initiator_info =
            prefetch_container->request().GetRendererInitiatorInfo()) {
      if (auto* rfh = renderer_initiator_info->GetRenderFrameHost()) {
        if (auto* web_contents = WebContents::FromRenderFrameHost(rfh)) {
          delegate_->OnPrefetchLikely(web_contents);
        }
      }
    }
  }

  if (GetInjectedEligibilityCheckForTesting()) {
    GetInjectedEligibilityCheckForTesting().Run(  // IN-TEST
        base::BindOnce(
            &PrefetchService::InjectedEligibilityCheckCompletedForTesting,
            weak_method_factory_.GetWeakPtr(), std::move(params)));
    return;
  }

  CheckEligibilityOfPrefetch(std::move(params));
}

void PrefetchService::InjectedEligibilityCheckCompletedForTesting(
    CheckEligibilityParams params,
    PreloadingEligibility eligibility) {
  if (!params.prefetch_container) {
    // The eligibility check can be paused and resumed via
    // `GetInjectedEligibilityCheckForTesting()`, so `prefetch_container` might
    // be already gone.
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  if (eligibility != PreloadingEligibility::kEligible) {
    std::move(params).Finish(eligibility);
    return;
  }
  CheckEligibilityOfPrefetch(std::move(params));
}

void PrefetchService::CheckEligibilityOfPrefetch(
    CheckEligibilityParams params) {
  const auto prefetch_container = params.prefetch_container;
  CHECK(prefetch_container);
  TRACE_EVENT_BEGIN("loading", "PrefetchService::CheckEligibility",
                    perfetto::Track::FromPointer(this));

  // TODO(crbug.com/40215782): Clean up the following checks by: 1)
  // moving each check to a separate function, and 2) requiring that failed
  // checks provide a PrefetchStatus related to the check.

  // While a registry-controlled domain could still resolve to a non-publicly
  // routable IP, this allows hosts which are very unlikely to work via the
  // proxy to be discarded immediately.
  //
  // Conditions on the outer-most if block:
  // Host-uniqueness check is only applied to proxied prefetches, where that
  // matters. Also, we bypass the check for the test hosts, since we run the
  // test web servers on the localhost or private networks, where the check
  // fails.
  if (params.IsProxyRequired()) {
    bool is_host_non_unique =
        g_host_non_unique_filter
            ? g_host_non_unique_filter(params.url.HostNoBrackets())
            : net::IsHostnameNonUnique(params.url.HostNoBrackets());
    if (is_host_non_unique) {
      std::move(params).Finish(PreloadingEligibility::kHostIsNonUnique);
      return;
    }
  }

  // Only HTTP(S) URLs which are believed to be secure are eligible.
  // For proxied prefetches, we only want HTTPS URLs.
  // For non-proxied prefetches, other URLs (notably localhost HTTP) is also
  // acceptable. This is common during development.
  const bool is_secure_http =
      params.IsProxyRequired()
          ? params.url.SchemeIs(url::kHttpsScheme)
          : (params.url.SchemeIsHTTPOrHTTPS() &&
             network::IsUrlPotentiallyTrustworthy(params.url));
  if (!is_secure_http) {
    std::move(params).Finish(PreloadingEligibility::kSchemeIsNotHttps);
    return;
  }

  // Fail the prefetch (or more precisely, PrefetchContainer::SinglePrefetch)
  // early if it is going to go through a proxy, and we know that it is not
  // available.
  if (params.IsProxyRequired() &&
      (!prefetch_proxy_configurator_ ||
       !prefetch_proxy_configurator_->IsPrefetchProxyAvailable())) {
    std::move(params).Finish(PreloadingEligibility::kPrefetchProxyNotAvailable);
    return;
  }

  // Only the default storage partition is supported since that is where we
  // check for service workers and existing cookies.
  StoragePartition* default_storage_partition =
      browser_context_->GetDefaultStoragePartition();
  if (default_storage_partition !=
      browser_context_->GetStoragePartitionForUrl(params.url,
                                                  /*can_create=*/false)) {
    std::move(params).Finish(
        PreloadingEligibility::kNonDefaultStoragePartition);
    return;
  }

  // If we have recently received a "retry-after" for the origin, then don't
  // send new prefetches.
  if (delegate_ && !delegate_->IsOriginOutsideRetryAfterWindow(params.url)) {
    std::move(params).Finish(PreloadingEligibility::kRetryAfter);
    return;
  }

  CheckHasServiceWorker(std::move(params));
}

void PrefetchService::CheckHasServiceWorker(CheckEligibilityParams params) {
  const auto prefetch_container = params.prefetch_container;
  CHECK(prefetch_container);
  TRACE_EVENT_BEGIN("loading", "PrefetchService::CheckHasServiceWorker",
                    perfetto::Track::FromPointer(this));

  if (params.is_redirect) {
    switch (prefetch_container->service_worker_state()) {
      case PrefetchServiceWorkerState::kDisallowed:
        break;

      case PrefetchServiceWorkerState::kAllowed:
        // Should have been transitioned out already.
        NOTREACHED();

      case PrefetchServiceWorkerState::kControlled:
        // Currently we disallow redirects from ServiceWorker-controlled
        // prefetches.
        std::move(params).Finish(
            PreloadingEligibility::kRedirectFromServiceWorker);
        return;
    }
  } else {
    switch (prefetch_container->service_worker_state()) {
      case PrefetchServiceWorkerState::kDisallowed:
        break;

      case PrefetchServiceWorkerState::kAllowed:
        // The controlling ServiceWorker will be checked by
        // `ServiceWorkerMainResourceLoaderInterceptor` from
        // `PrefetchStreamingURLLoader`, not here during eligibility check.
        OnGotServiceWorkerResult(std::move(params), base::Time::Now(),
                                 ServiceWorkerCapability::NO_SERVICE_WORKER);
        return;

      case PrefetchServiceWorkerState::kControlled:
        NOTREACHED();
    }
  }

  // This service worker check assumes that the prefetch will only ever be
  // performed in a first-party context (main frame prefetch). At the moment
  // that is true but if it ever changes then the StorageKey will need to be
  // constructed with the top-level site to ensure correct partitioning.
  ServiceWorkerContext* service_worker_context =
      g_service_worker_context_for_testing
          ? g_service_worker_context_for_testing
          : browser_context_->GetDefaultStoragePartition()
                ->GetServiceWorkerContext();
  CHECK(service_worker_context);
  auto key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(params.url));
  // Check `MaybeHasRegistrationForStorageKey` first as it is much faster than
  // calling `CheckHasServiceWorker`.
  auto has_registration_for_storage_key =
      service_worker_context->MaybeHasRegistrationForStorageKey(key);
  if (auto* preloading_attempt = static_cast<PreloadingAttemptImpl*>(
          prefetch_container->request().attempt())) {
    preloading_attempt->SetServiceWorkerRegisteredCheck(
        has_registration_for_storage_key
            ? PreloadingAttemptImpl::ServiceWorkerRegisteredCheck::kPath
            : PreloadingAttemptImpl::ServiceWorkerRegisteredCheck::kOriginOnly);
  }
  if (!has_registration_for_storage_key) {
    OnGotServiceWorkerResult(std::move(params), base::Time::Now(),
                             ServiceWorkerCapability::NO_SERVICE_WORKER);
    return;
  }
  // Start recording here the start of the check for Service Worker registration
  // for url.
  // `url` is needed to avoid use-after-move.
  const GURL url = params.url;
  service_worker_context->CheckHasServiceWorker(
      url, key,
      base::BindOnce(&PrefetchService::OnGotServiceWorkerResult,
                     weak_method_factory_.GetWeakPtr(), std::move(params),
                     base::Time::Now()));
}

void PrefetchService::OnGotServiceWorkerResult(
    CheckEligibilityParams params,
    base::Time check_has_service_worker_start_time,
    ServiceWorkerCapability service_worker_capability) {
  const auto prefetch_container = params.prefetch_container;

  // End "PrefetchService::CheckHasServiceWorker" trace event.
  TRACE_EVENT_END("loading", perfetto::Track::FromPointer(this));
  TRACE_EVENT("loading", "PrefetchService::OnGotServiceWorkerResult",
              "prefetch_url",
              prefetch_container ? prefetch_container->GetURL().spec() : "");
  if (!prefetch_container) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }
  if (auto* preloading_attempt = static_cast<PreloadingAttemptImpl*>(
          prefetch_container->request().attempt())) {
    const auto duration =
        base::Time::Now() - check_has_service_worker_start_time;
    preloading_attempt->SetServiceWorkerRegisteredCheckDuration(duration);
  }
  // Note that after ServiceWorker+Prefetch support is implemented,
  // - For ServiceWorker-eligible prefetches,
  //   `ServiceWorkerCapability::NO_SERVICE_WORKER` is passed here and thus the
  //   ServiceWorker-related ineligibility values here are not used.
  // - For ServiceWorker-ineligible prefetches (e.g. cross-site prefetches),
  //   they still goes through the checks below and the ServiceWorker-related
  //   ineligibility values here are still valid and used.
  switch (service_worker_capability) {
    case ServiceWorkerCapability::NO_SERVICE_WORKER:
      break;
    case ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER:
      if (base::FeatureList::IsEnabled(
              features::kPrefetchServiceWorkerNoFetchHandlerFix)) {
        std::move(params).Finish(
            PreloadingEligibility::kUserHasServiceWorkerNoFetchHandler);
        return;
      }
      break;
    case ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER: {
      std::move(params).Finish(
          params.is_redirect ? PreloadingEligibility::kRedirectToServiceWorker
                             : PreloadingEligibility::kUserHasServiceWorker);
      return;
    }
  }
  // This blocks same-site cross-origin prefetches that require the prefetch
  // proxy. Same-site prefetches are made using the default network context, and
  // the prefetch request cannot be configured to use the proxy in that network
  // context.
  // TODO(crbug.com/40265797): Allow same-site cross-origin prefetches
  // that require the prefetch proxy to be made.
  if (params.IsProxyRequired() &&
      !prefetch_container
           ->IsIsolatedNetworkContextRequiredForCurrentPrefetch()) {
    std::move(params).Finish(
        PreloadingEligibility::kSameSiteCrossOriginPrefetchRequiredProxy);
    return;
  }
  // We do not need to check the cookies of prefetches that do not need an
  // isolated network context.
  if (!prefetch_container
           ->IsIsolatedNetworkContextRequiredForCurrentPrefetch()) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  StoragePartition* default_storage_partition =
      browser_context_->GetDefaultStoragePartition();
  CHECK(default_storage_partition);
  TRACE_EVENT_BEGIN("loading", "PrefetchService::CheckCookies",
                    perfetto::Track::FromPointer(this));
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  options.set_return_excluded_cookies();
  // `url` is needed to avoid use-after-move.
  const GURL url = params.url;
  default_storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      url, options, net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&PrefetchService::OnGotCookiesForEligibilityCheck,
                     weak_method_factory_.GetWeakPtr(), std::move(params)));
}

void PrefetchService::OnGotCookiesForEligibilityCheck(
    CheckEligibilityParams params,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  const auto prefetch_container = params.prefetch_container;

  // End "PrefetchService::CheckCookies" trace event.
  TRACE_EVENT_END("loading", perfetto::Track::FromPointer(this));
  TRACE_EVENT("loading", "PrefetchService::OnGotCookiesForEligibilityCheck",
              "prefetch_url",
              prefetch_container ? prefetch_container->GetURL().spec() : "");
  if (!prefetch_container) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  if (!cookie_list.empty()) {
    std::move(params).Finish(PreloadingEligibility::kUserHasCookies);
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kPrefetchStateContaminationMitigation)) {
    // The cookie eligibility check just happened, and we might proceed anyway.
    // We might therefore need to delay further processing to the extent
    // required to obscure the outcome of this check from the current site.
    const bool is_contamination_exempt =
        delegate_ &&
        prefetch_container->request().referring_origin().has_value() &&
        delegate_->IsContaminationExempt(
            prefetch_container->request().referring_origin().value());
    if (!is_contamination_exempt) {
      prefetch_container->MarkCrossSiteContaminated();
    }
  }

  // Cookies are tricky because cookies for different paths or a higher level
  // domain (e.g.: m.foo.com and foo.com) may not show up in |cookie_list|, but
  // they will show up in |excluded_cookies|. To check for any cookies for a
  // domain, compare the domains of the prefetched |url| and the domains of all
  // the returned cookies.
  bool excluded_cookie_has_tld = false;
  for (const auto& cookie_result : excluded_cookies) {
    if (cookie_result.cookie.IsExpired(base::Time::Now())) {
      // Expired cookies don't count.
      continue;
    }

    if (params.url.DomainIs(cookie_result.cookie.DomainWithoutDot())) {
      excluded_cookie_has_tld = true;
      break;
    }
  }

  if (excluded_cookie_has_tld) {
    std::move(params).Finish(PreloadingEligibility::kUserHasCookies);
    return;
  }

  StartProxyLookupCheck(std::move(params));
}

void PrefetchService::StartProxyLookupCheck(CheckEligibilityParams params) {
  const auto prefetch_container = params.prefetch_container;
  // Same origin prefetches (which use the default network context and cannot
  // use the prefetch proxy) can use the existing proxy settings.
  // TODO(crbug.com/40231580): Copy proxy settings over to the isolated
  // network context for the prefetch in order to allow non-private cross origin
  // prefetches to be made using the existing proxy settings.
  if (!prefetch_container
           ->IsIsolatedNetworkContextRequiredForCurrentPrefetch()) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  TRACE_EVENT_BEGIN("loading", "PrefetchService::ProxyCheck",
                    perfetto::Track::FromPointer(this));
  // Start proxy check for this prefetch, and give ownership of the
  // |ProxyLookupClientImpl| to |prefetch_container|.
  // `url` is needed to avoid use-after-move.
  const GURL url = params.url;
  prefetch_container->TakeProxyLookupClient(
      std::make_unique<ProxyLookupClientImpl>(
          url,
          base::BindOnce(&PrefetchService::OnGotProxyLookupResult,
                         weak_method_factory_.GetWeakPtr(), std::move(params)),
          g_network_context_for_proxy_lookup_for_testing
              ? g_network_context_for_proxy_lookup_for_testing
              : browser_context_->GetDefaultStoragePartition()
                    ->GetNetworkContext()));
}

void PrefetchService::OnGotProxyLookupResult(CheckEligibilityParams params,
                                             bool has_proxy) {
  const auto prefetch_container = params.prefetch_container;
  // End "PrefetchService::ProxyCheck" trace event.
  TRACE_EVENT_END("loading", perfetto::Track::FromPointer(this));
  TRACE_EVENT("loading", "PrefetchService::OnGotProxyLookupResult",
              "prefetch_url",
              prefetch_container ? prefetch_container->GetURL().spec() : "");
  if (!prefetch_container) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  prefetch_container->ReleaseProxyLookupClient();
  if (has_proxy) {
    std::move(params).Finish(PreloadingEligibility::kExistingProxy);
    return;
  }

  std::move(params).Finish(PreloadingEligibility::kEligible);
}

void PrefetchService::OnGotEligibilityForNonRedirect(
    CheckEligibilityParams params,
    PreloadingEligibility eligibility) {
  const auto prefetch_container = params.prefetch_container;
  // End "PrefetchService::CheckEligibility" trace event.
  TRACE_EVENT_END("loading", perfetto::Track::FromPointer(this));
  TRACE_EVENT("loading", "PrefetchService::OnGotEligibilityForNonRedirect",
              "prefetch_url",
              prefetch_container ? prefetch_container->GetURL().spec() : "");
  if (!prefetch_container) {
    return;
  }

  const bool eligible = eligibility == PreloadingEligibility::kEligible;
  bool is_decoy = false;
  if (!eligible) {
    is_decoy =
        params.IsProxyRequired() &&
        ShouldConsiderDecoyRequestForStatus(eligibility) &&
        PrefetchServiceSendDecoyRequestForIneligblePrefetch(
            delegate_ ? delegate_->DisableDecoysBasedOnUserSettings() : false);
  }
  // The prefetch decoy is pushed onto the queue and the network request will be
  // dispatched, but the response will not be used. Thus it is eligible but a
  // failure.
  prefetch_container->SetIsDecoy(is_decoy);
  if (is_decoy) {
    prefetch_container->OnEligibilityCheckComplete(
        PreloadingEligibility::kEligible);
  } else {
    prefetch_container->OnEligibilityCheckComplete(eligibility);
  }

  if (!eligible && !is_decoy) {
    DVLOG(1)
        << *prefetch_container
        << ": not prefetched (not eligible nor decoy. PreloadingEligibility="
        << static_cast<int>(eligibility) << ")";
    return;
  }

  if (!is_decoy) {
    prefetch_container->SetPrefetchStatus(PrefetchStatus::kPrefetchNotStarted);

    // Registers a cookie listener for this prefetch if it is using an isolated
    // network context. If the cookies in the default partition associated with
    // this URL change after this point, then the prefetched resources should
    // not be served.
    if (prefetch_container
            ->IsIsolatedNetworkContextRequiredForCurrentPrefetch()) {
      prefetch_container->RegisterCookieListener(
          browser_context_->GetDefaultStoragePartition()
              ->GetCookieManagerForBrowserProcess());
    }
  }

  if (!UsePrefetchScheduler()) {
    prefetch_queue_.push_back(std::move(prefetch_container));
    Prefetch();
  } else {
    if (features::kPrefetchSchedulerProgressSyncBestEffort.Get()) {
      ScheduleAndProgress(std::move(prefetch_container));
    } else {
      ScheduleAndProgressAsync(std::move(prefetch_container));
    }
  }
}

void PrefetchService::OnGotEligibilityForRedirect(
    net::RedirectInfo redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head,
    CheckEligibilityParams params,
    PreloadingEligibility eligibility) {
  const auto prefetch_container = params.prefetch_container;
  // End "PrefetchService::CheckEligibility" trace event.
  TRACE_EVENT_END("loading", perfetto::Track::FromPointer(this));
  TRACE_EVENT("loading", "PrefetchService::OnGotEligibilityForRedirect",
              "prefetch_url",
              prefetch_container ? prefetch_container->GetURL().spec() : "");
  if (!prefetch_container) {
    return;
  }

  // Returns `false` if `OnGotEligibilityForRedirect()` should be early-returned
  // because the prefetch was already terminated during the eligiblity check.
  const auto check_streaming_loader = [&]() {
    // TODO(crbug.com/396133768): Consider setting appropriate PrefetchStatus.
    auto streaming_url_loader = prefetch_container->GetStreamingURLLoader();
    if (streaming_url_loader) {
      return true;
    }
    if (!UsePrefetchScheduler()) {
      if (active_prefetch_ == prefetch_container->key()) {
        active_prefetch_ = std::nullopt;
        Prefetch();
      }
    } else {
      // TODO(crbug.com/400761083): Use
      // `ResetPrefetchContainerAndProgressAsync()` instead.
      RemoveFromSchedulerAndProgressAsync(*prefetch_container);
    }
    return false;
  };

  if (base::FeatureList::IsEnabled(features::kPrefetchGracefulNotification)) {
    if (!check_streaming_loader()) {
      // TODO(crbug.com/400761083): Turn this into `CHECK_EQ()`.
      DUMP_WILL_BE_CHECK_EQ(prefetch_container->GetLoadState(),
                            PrefetchContainer::LoadState::kFailed);
      return;
    }
  }

  const bool eligible = eligibility == PreloadingEligibility::kEligible;
  RecordRedirectResult(eligible
                           ? PrefetchRedirectResult::kSuccessRedirectFollowed
                           : PrefetchRedirectResult::kFailedIneligible);

  // If the redirect is ineligible, the prefetch may change into a decoy.
  bool is_decoy = false;
  if (!eligible) {
    is_decoy =
        params.IsProxyRequired() &&
        ShouldConsiderDecoyRequestForStatus(eligibility) &&
        PrefetchServiceSendDecoyRequestForIneligblePrefetch(
            delegate_ ? delegate_->DisableDecoysBasedOnUserSettings() : false);
  }
  prefetch_container->SetIsDecoy(prefetch_container->IsDecoy() || is_decoy);

  // Inform the prefetch container of the result of the eligibility check
  if (prefetch_container->IsDecoy()) {
    prefetch_container->OnEligibilityCheckComplete(
        PreloadingEligibility::kEligible);
  } else {
    prefetch_container->OnEligibilityCheckComplete(eligibility);
    if (eligible &&
        prefetch_container
            ->IsIsolatedNetworkContextRequiredForCurrentPrefetch()) {
      prefetch_container->RegisterCookieListener(
          browser_context_->GetDefaultStoragePartition()
              ->GetCookieManagerForBrowserProcess());
    }
  }

  if (!base::FeatureList::IsEnabled(features::kPrefetchGracefulNotification)) {
    if (!check_streaming_loader()) {
      return;
    }
  }
  auto streaming_url_loader = prefetch_container->GetStreamingURLLoader();
  CHECK(streaming_url_loader);

  // If the redirect is not eligible and the prefetch is not a decoy, then stop
  // the prefetch.
  if (!eligible && !prefetch_container->IsDecoy()) {
    CHECK(IsPrefetchContainerInActiveSet(*prefetch_container));

    if (!UsePrefetchScheduler()) {
      active_prefetch_ = std::nullopt;
      streaming_url_loader->HandleRedirect(PrefetchRedirectStatus::kFail,
                                           redirect_info,
                                           std::move(redirect_head));

      Prefetch();
    } else {
      // Remove first as it requires that `PrefetchContainer` is available.
      RemoveFromSchedulerAndProgressAsync(*prefetch_container);

      streaming_url_loader->HandleRedirect(PrefetchRedirectStatus::kFail,
                                           redirect_info,
                                           std::move(redirect_head));

      // TODO(crbug.com/400761083): Use
      // `ResetPrefetchContainerAndProgressAsync()` instead.
    }
    return;
  }

  prefetch_container->NotifyPrefetchRequestWillBeSent(&redirect_head);

  // If the redirect requires a change in network contexts, then stop the
  // current streaming URL loader and start a new streaming URL loader for the
  // redirect URL.
  if (prefetch_container
          ->IsIsolatedNetworkContextRequiredForCurrentPrefetch() !=
      prefetch_container
          ->IsIsolatedNetworkContextRequiredForPreviousRedirectHop()) {
    streaming_url_loader->HandleRedirect(
        PrefetchRedirectStatus::kSwitchNetworkContext, redirect_info,
        std::move(redirect_head));
    // The new ResponseReader is associated with the new streaming URL loader at
    // the PrefetchStreamingURLLoader constructor.
    SendPrefetchRequest(prefetch_container);

    return;
  }

  // Otherwise, follow the redirect in the same streaming URL loader.
  streaming_url_loader->HandleRedirect(PrefetchRedirectStatus::kFollow,
                                       redirect_info, std::move(redirect_head));
  // Associate the new ResponseReader with the current streaming URL loader.
  streaming_url_loader->SetResponseReader(
      prefetch_container->GetResponseReaderForCurrentPrefetch());
}

void PrefetchService::Prefetch() {
  CHECK(!UsePrefetchScheduler());

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

// Asserts that re-entrancy doesn't happen.
#if DCHECK_IS_ON()
  DCHECK(!prefetch_reentrancy_guard_);
  base::AutoReset reset_guard(&prefetch_reentrancy_guard_, true);
#endif

  PrepareProgress();

  base::WeakPtr<PrefetchContainer> next_prefetch = nullptr;
  base::WeakPtr<PrefetchContainer> prefetch_to_evict = nullptr;
  while ((std::tie(next_prefetch, prefetch_to_evict) =
              PopNextPrefetchContainer()) !=
         std::make_tuple(nullptr, nullptr)) {
    if (prefetch_to_evict) {
      EvictPrefetch(std::move(prefetch_to_evict));
    }
    StartSinglePrefetch(next_prefetch);
  }
}

std::tuple<base::WeakPtr<PrefetchContainer>, base::WeakPtr<PrefetchContainer>>
PrefetchService::PopNextPrefetchContainer() {
  CHECK(!UsePrefetchScheduler());

  auto new_end = std::remove_if(
      prefetch_queue_.begin(), prefetch_queue_.end(),
      [&](const base::WeakPtr<PrefetchContainer>& prefetch_container) {
        // Remove all prefetches from queue that no longer exist.
        return !prefetch_container;
      });
  prefetch_queue_.erase(new_end, prefetch_queue_.end());

  // Don't start any new prefetches if we are currently running one.
  if (active_prefetch_.has_value()) {
    DVLOG(1) << "PrefetchService::PopNextPrefetchContainer: already running a "
                "prefetch.";
    return std::make_tuple(nullptr, nullptr);
  }

  base::WeakPtr<PrefetchContainer> prefetch_to_evict;
  // Get the first prefetch can be prefetched currently. For the triggers
  // managed by PrefetchDocumentManager, this depends on the state of the
  // initiating document, and the number of completed prefetches (this can also
  // result in previously completed prefetches being evicted).
  auto prefetch_iter = std::ranges::find_if(
      prefetch_queue_,
      [&](const base::WeakPtr<PrefetchContainer>& prefetch_container) {
        // Keep this method as similar as much as possible to
        // `IsReadyToStartLoading` in
        // //content/browser/preloading/prefetch/prefetch_scheduler.cc.

        auto* renderer_initiator_info =
            prefetch_container->request().GetRendererInitiatorInfo();
        if (!renderer_initiator_info) {
          // TODO(crbug.com/40946257): Revisit the resource limits and
          // conditions for starting browser-initiated prefetch.
          return true;
        }

        auto* prefetch_document_manager =
            renderer_initiator_info->prefetch_document_manager();
        // If there is no manager in renderer-initiated prefetch (can happen
        // only in tests), just bypass the check.
        if (!prefetch_document_manager) {
          CHECK_IS_TEST();
          return true;
        }
        bool can_prefetch_now = false;
        std::tie(can_prefetch_now, prefetch_to_evict) =
            prefetch_document_manager->CanPrefetchNow(prefetch_container.get());
        // |prefetch_to_evict| should only be set if |can_prefetch_now| is true.
        DCHECK(!prefetch_to_evict || can_prefetch_now);
        return can_prefetch_now;
      });
  if (prefetch_iter == prefetch_queue_.end()) {
    return std::make_tuple(nullptr, nullptr);
  }

  base::WeakPtr<PrefetchContainer> next_prefetch_container = *prefetch_iter;
  prefetch_queue_.erase(prefetch_iter);
  return std::make_tuple(next_prefetch_container, prefetch_to_evict);
}

void PrefetchService::OnPrefetchTimeout(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  prefetch_container->SetPrefetchStatus(PrefetchStatus::kPrefetchIsStale);
  if (!UsePrefetchScheduler()) {
    ResetPrefetchContainer(prefetch_container);

    if (!active_prefetch_) {
      Prefetch();
    }
  } else {
    ResetPrefetchContainerAndProgressAsync(std::move(prefetch_container));
  }
}

void PrefetchService::MayReleasePrefetch(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  if (!prefetch_container) {
    return;
  }

  if (!base::Contains(owned_prefetches(), prefetch_container->key())) {
    return;
  }

  if (!UsePrefetchScheduler()) {
    ResetPrefetchContainer(prefetch_container);
    if (!active_prefetch_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&PrefetchService::Prefetch,
                                    weak_method_factory_.GetWeakPtr()));
    }
  } else {
    ResetPrefetchContainerAndProgressAsync(std::move(prefetch_container));
  }
}

void PrefetchService::ResetPrefetchContainer(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    bool should_progress) {
  CHECK(prefetch_container);

  if (!UsePrefetchScheduler()) {
    if (active_prefetch_ == prefetch_container->key()) {
      active_prefetch_ = std::nullopt;
    }
  } else {
    // Remove before calling `PrefetchContainer::dtor()` as `PrefetchScheduler`
    // manages them with weak pointers.
    scheduler_->RemoveAndProgressAsync(*prefetch_container, should_progress);
  }

  auto it = owned_prefetches().find(prefetch_container->key());
  CHECK(it != owned_prefetches().end());
  CHECK_EQ(it->second.get(), prefetch_container.get());
  owned_prefetches_.erase(it);
}

void PrefetchService::ScheduleAndProgress(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  CHECK(UsePrefetchScheduler());
  CHECK(prefetch_container);

  scheduler_->PushAndProgress(*prefetch_container);
}

void PrefetchService::ScheduleAndProgressAsync(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  CHECK(UsePrefetchScheduler());
  CHECK(prefetch_container);

  scheduler_->PushAndProgressAsync(*prefetch_container);

  // `PrefetchScheduler::Progress()` will be called asynchronously.
}

void PrefetchService::ResetPrefetchContainerAndProgressAsync(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  CHECK(UsePrefetchScheduler());

  ResetPrefetchContainer(std::move(prefetch_container));

  // `PrefetchScheduler::Progress()` will be called asynchronously.
}

void PrefetchService::ResetPrefetchContainersAndProgressAsync(
    std::vector<base::WeakPtr<PrefetchContainer>> prefetch_containers) {
  CHECK(UsePrefetchScheduler());

  for (auto& prefetch_container : prefetch_containers) {
    ResetPrefetchContainer(std::move(prefetch_container));
  }

  // `PrefetchScheduler::Progress()` will be called asynchronously.
}

void PrefetchService::RemoveFromSchedulerAndProgressAsync(
    PrefetchContainer& prefetch_container) {
  CHECK(UsePrefetchScheduler());

  scheduler_->RemoveAndProgressAsync(prefetch_container);

  // `PrefetchScheduler::Progress()` will be called asynchronously.
}

void PrefetchService::OnCandidatesUpdated() {
  if (!UsePrefetchScheduler()) {
    if (!active_prefetch_) {
      Prefetch();
    }
  } else {
    // Before `kPrefetchScheduler`, calling `Prefetch()` here was necessary to
    // progress scheduling as modifying `PrefetchService::queue_` doesn't set
    // `active_prefetch_`.
    //
    // After `kPrefetchScheduler`, `PrefetchScheduler` ensures that modifying
    // `PrefetchQueue` triggers `PrefetchScheduler::Progress()` eventually. So,
    // we believe that this explicit `Progress()` call is not necessary. But we
    // keep it because 1. It's safe (as it's not reentrancy) and noop if not
    // necessary. 2. We should another experiment to remove the call as we are
    // using `PrefetchScheduler` in some experiments.
    //
    // TODO(crbug.com/406754449): Consider to remove it.
    scheduler_->Progress();
  }
}

void PrefetchService::AddRecentUnmatchedNavigatedKeysForMetrics(
    const PrefetchKey& navigated_key) {
  recent_unmatched_navigated_keys_for_metrics_.Put(navigated_key,
                                                   base::TimeTicks::Now());
}

void PrefetchService::PrepareProgress(base::PassKey<PrefetchScheduler>) {
  PrepareProgress();
}

void PrefetchService::PrepareProgress() {
  // Corresponds to the same code in `Prefetch()`.
  //
  // For necessity, see
  // https://docs.google.com/document/d/1U1J8VvvVhhLL2YQSRDPH2x4tH-Vl38UjJvojMmENK0I
  //
  // TODO(crbug.com/443681583): Move the handling to `PrefetchContainer` if
  // possible.
  for (const auto& iter : owned_prefetches()) {
    if (iter.second) {
      iter.second->CloseIdleConnections();
    }
  }
}

void PrefetchService::EvictPrefetch(
    base::PassKey<PrefetchScheduler>,
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  EvictPrefetch(std::move(prefetch_container));
}

void PrefetchService::EvictPrefetch(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  CHECK(prefetch_container);

  prefetch_container->SetPrefetchStatus(
      PrefetchStatus::kPrefetchEvictedForNewerPrefetch);
  ResetPrefetchContainer(std::move(prefetch_container));
}

bool PrefetchService::StartSinglePrefetch(
    base::PassKey<PrefetchScheduler>,
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  return StartSinglePrefetch(std::move(prefetch_container));
}

bool PrefetchService::StartSinglePrefetchForTesting(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  return StartSinglePrefetch(std::move(prefetch_container));
}

bool PrefetchService::StartSinglePrefetch(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(prefetch_container);
  CHECK_EQ(prefetch_container->GetLoadState(),
           PrefetchContainer::LoadState::kEligible);

  // Do not prefetch for a Holdback control group. Called after the checks in
  // `PopNextPrefetchContainer` because we want to compare against the
  // prefetches that would have been dispatched.
  if (CheckAndSetPrefetchHoldbackStatus(prefetch_container)) {
    DVLOG(1) << *prefetch_container
             << ": not prefetched (holdback control group)";
    return false;
  }

  prefetch_container->OnPrefetchStarted();

  // Checks if the `PrefetchContainer` has a specific TTL (Time-to-Live)
  // configured. If a TTL is configured, the prefetch container will be eligible
  // for removal after the TTL expires. Otherwise, it will remain alive
  // indefinitely.
  //
  // The default TTL is determined by
  // `PrefetchContainerDefaultTtlInPrefetchService()`, which may return a zero
  // or negative value, indicating an indefinite TTL.
  prefetch_container->StartTimeoutTimerIfNeeded(
      base::BindOnce(&PrefetchService::OnPrefetchTimeout,
                     weak_method_factory_.GetWeakPtr(), prefetch_container));

  if (!UsePrefetchScheduler()) {
    active_prefetch_.emplace(prefetch_container->key());
  }

  if (!prefetch_container->IsDecoy()) {
    // The status is updated to be successful or failed when it finishes.
    prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchNotFinishedInTime);
  }

  prefetch_container->MakeResourceRequest();

  prefetch_container->NotifyPrefetchRequestWillBeSent(
      /*redirect_head=*/nullptr);

  SendPrefetchRequest(prefetch_container);

  PrefetchDocumentManager* prefetch_document_manager = nullptr;
  if (auto* renderer_initiator_info =
          prefetch_container->request().GetRendererInitiatorInfo()) {
    prefetch_document_manager =
        renderer_initiator_info->prefetch_document_manager();
  }

  if (prefetch_container->request()
          .prefetch_type()
          .IsProxyRequiredWhenCrossOrigin() &&
      !prefetch_container->IsDecoy() &&
      (!prefetch_document_manager ||
       !prefetch_document_manager->HaveCanaryChecksStarted())) {
    // TODO(crbug.com/40946257): Currently browser-initiated prefetch will
    // always perform canary checks since there is no PrefetchDocumentManager.
    // Revisit and add proper handlings.

    // Make sure canary checks have run so we know the result by the time we
    // want to use the prefetch. Checking the canary cache can be a slow and
    // blocking operation (see crbug.com/1266018), so we only do this for the
    // first non-decoy prefetch we make on the page.
    // TODO(crbug.com/40801832): once this bug is fixed, fire off canary check
    // regardless of whether the request is a decoy or not.
    origin_prober_->RunCanaryChecksIfNeeded();

    if (prefetch_document_manager) {
      prefetch_document_manager->OnCanaryChecksStarted();
    }
  }

  // Start a spare renderer now so that it will be ready by the time it is
  // useful to have.
  if (ShouldStartSpareRenderer()) {
    SpareRenderProcessHostManager::Get().WarmupSpare(browser_context_);
  }

  MaybeSetPrefetchMatchMissedTimeForMetrics(prefetch_container);

  return true;
}

void PrefetchService::SendPrefetchRequest(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  TRACE_EVENT("loading", "PrefetchService::SendPrefetchRequest");
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("speculation_rules_prefetch",
                                          R"(
          semantics {
            sender: "Speculation Rules Prefetch Loader"
            description:
              "Prefetches the mainframe HTML of a page specified via "
              "speculation rules. This is done out-of-band of normal "
              "prefetches to allow total isolation of this request from the "
              "rest of browser traffic and user state like cookies and cache."
            trigger:
              "Used only when this feature and speculation rules feature are "
              "enabled."
            data: "None."
            destination: WEBSITE
          }
          policy {
            cookies_allowed: NO
            setting:
              "Users can control this via a setting specific to each content "
              "embedder."
            policy_exception_justification: "Not implemented."
        })");

  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      GetURLLoaderFactoryForCurrentPrefetch(prefetch_container),
      *prefetch_container->GetResourceRequest(), traffic_annotation,
      PrefetchTimeoutDuration(),
      base::BindOnce(&PrefetchService::OnPrefetchResponseStarted,
                     base::Unretained(this), prefetch_container),
      base::BindRepeating(&PrefetchService::OnPrefetchRedirect,
                          base::Unretained(this), prefetch_container),
      prefetch_container->GetResponseReaderForCurrentPrefetch(),
      prefetch_container->service_worker_state(), browser_context_,
      base::BindOnce(&PrefetchContainer::OnServiceWorkerStateDetermined,
                     prefetch_container));
  prefetch_container->SetStreamingURLLoader(std::move(streaming_loader));

  DVLOG(1) << *prefetch_container << ": PrefetchStreamingURLLoader is created.";
}

void PrefetchService::MaybeSetPrefetchMatchMissedTimeForMetrics(
    base::WeakPtr<PrefetchContainer> prefetch_container) const {
  // Check if the prefetch could've been matched to a recently navigated URL.
  // We will log the event to UMA in order to understand if a potential race
  // condition has occurred.
  // TODO(crbug.com/433478563): Introduce another variant of
  // `no_vary_search::MatchUrl` that doesn't require a map.
  std::map<PrefetchKey, base::WeakPtr<PrefetchContainer>> candidates{
      {prefetch_container->key(), prefetch_container}};
  for (auto& it : recent_unmatched_navigated_keys_for_metrics_) {
    bool urls_match = !!no_vary_search::MatchUrl(it.first, candidates);
    if (urls_match) {
      prefetch_container->SetPrefetchMatchMissedTimeForMetrics(it.second);
      break;
    }
  }
}

scoped_refptr<network::SharedURLLoaderFactory>
PrefetchService::GetURLLoaderFactoryForCurrentPrefetch(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  DCHECK(prefetch_container);
  if (g_url_loader_factory_for_testing) {
    return base::WrapRefCounted(g_url_loader_factory_for_testing);
  }
  return prefetch_container->GetOrCreateNetworkContextForCurrentPrefetch()
      ->GetURLLoaderFactory(this);
}

void PrefetchService::OnPrefetchRedirect(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  TRACE_EVENT("loading", "PrefetchService::OnPrefetchRedirect", "prefetch_url",
              prefetch_container ? prefetch_container->GetURL().spec() : "");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!prefetch_container) {
    RecordRedirectResult(PrefetchRedirectResult::kFailedNullPrefetch);
    return;
  }

  CHECK(IsPrefetchContainerInActiveSet(*prefetch_container));

  // Update the prefetch's referrer in case a redirect requires a change in
  // network context and a new request needs to be started.
  const auto new_referrer_policy =
      blink::ReferrerUtils::NetToMojoReferrerPolicy(
          redirect_info.new_referrer_policy);

  std::optional<PrefetchRedirectResult> failure;
  if (redirect_info.new_method != "GET") {
    failure = PrefetchRedirectResult::kFailedInvalidMethod;
  } else if (!redirect_head->headers ||
             redirect_head->headers->response_code() < 300 ||
             redirect_head->headers->response_code() >= 400) {
    failure = PrefetchRedirectResult::kFailedInvalidResponseCode;
  } else if (!net::SchemefulSite::IsSameSite(
                 prefetch_container->GetCurrentURL(), redirect_info.new_url) &&
             !IsReferrerPolicySufficientlyStrict(new_referrer_policy)) {
    // The new referrer policy is not sufficiently strict to allow cross-site
    // redirects.
    failure = PrefetchRedirectResult::kFailedInsufficientReferrerPolicy;
  }

  if (failure) {
    CHECK(IsPrefetchContainerInActiveSet(*prefetch_container));

    if (!UsePrefetchScheduler()) {
      active_prefetch_ = std::nullopt;
      prefetch_container->SetPrefetchStatus(
          PrefetchStatus::kPrefetchFailedInvalidRedirect);
      if (auto streaming_url_loader =
              prefetch_container->GetStreamingURLLoader()) {
        streaming_url_loader->HandleRedirect(PrefetchRedirectStatus::kFail,
                                             redirect_info,
                                             std::move(redirect_head));
      }

      Prefetch();
      RecordRedirectResult(*failure);
    } else {
      RecordRedirectResult(*failure);

      prefetch_container->SetPrefetchStatus(
          PrefetchStatus::kPrefetchFailedInvalidRedirect);

      // Remove first as it requires that `PrefetchContainer` is available.
      RemoveFromSchedulerAndProgressAsync(*prefetch_container);

      if (auto streaming_url_loader =
              prefetch_container->GetStreamingURLLoader()) {
        streaming_url_loader->HandleRedirect(PrefetchRedirectStatus::kFail,
                                             redirect_info,
                                             std::move(redirect_head));
      }

      // TODO(crbug.com/400761083): Use
      // `ResetPrefetchContainerAndProgressAsync()` instead.
    }
    return;
  }

  prefetch_container->AddRedirectHop(redirect_info);
  prefetch_container->UpdateReferrer(GURL(redirect_info.new_referrer),
                                     new_referrer_policy);

  RecordRedirectNetworkContextTransition(
      prefetch_container
          ->IsIsolatedNetworkContextRequiredForPreviousRedirectHop(),
      prefetch_container->IsIsolatedNetworkContextRequiredForCurrentPrefetch());

  auto params = CheckEligibilityParams(
      {.prefetch_container = prefetch_container,
       .url = redirect_info.new_url,
       .is_redirect = true,
       .callback = base::BindOnce(&PrefetchService::OnGotEligibilityForRedirect,
                                  weak_method_factory_.GetWeakPtr(),
                                  redirect_info, std::move(redirect_head))});

  if (GetInjectedEligibilityCheckForTesting()) {
    GetInjectedEligibilityCheckForTesting().Run(  // IN-TEST
        base::BindOnce(
            &PrefetchService::InjectedEligibilityCheckCompletedForTesting,
            weak_method_factory_.GetWeakPtr(), std::move(params)));
    return;
  }

  CheckEligibilityOfPrefetch(std::move(params));
}

std::optional<PrefetchErrorOnResponseReceived>
PrefetchService::OnPrefetchResponseStarted(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    network::mojom::URLResponseHead* head) {
  TRACE_EVENT("loading", "PrefetchService::OnPrefetchResponseStarted",
              "prefetch_url",
              prefetch_container ? prefetch_container->GetURL().spec() : "");
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!prefetch_container || prefetch_container->IsDecoy()) {
    return PrefetchErrorOnResponseReceived::kPrefetchWasDecoy;
  }

  if (!head) {
    return PrefetchErrorOnResponseReceived::kFailedInvalidHead;
  }

  if (prefetch_container && prefetch_container->IsCrossSiteContaminated()) {
    head->is_prefetch_with_cross_site_contamination = true;
  }

  prefetch_container->NotifyPrefetchResponseReceived(*head);

  if (!head->headers) {
    return PrefetchErrorOnResponseReceived::kFailedInvalidHeaders;
  }

  RecordPrefetchProxyPrefetchMainframeTotalTime(head);
  RecordPrefetchProxyPrefetchMainframeConnectTime(head);

  int response_code = head->headers->response_code();
  RecordPrefetchProxyPrefetchMainframeRespCode(response_code);
  if (response_code < 200 || response_code >= 300) {
    prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchFailedNon2XX);

    if (response_code == net::HTTP_SERVICE_UNAVAILABLE) {
      base::TimeDelta retry_after;
      std::string retry_after_string;
      if (head->headers->EnumerateHeader(nullptr, "Retry-After",
                                         &retry_after_string) &&
          net::HttpUtil::ParseRetryAfterHeader(
              retry_after_string, base::Time::Now(), &retry_after) &&
          delegate_) {
        // Cap the retry after value to a maximum.
        static constexpr base::TimeDelta max_retry_after = base::Days(7);
        if (retry_after > max_retry_after) {
          retry_after = max_retry_after;
        }

        delegate_->ReportOriginRetryAfter(prefetch_container->GetURL(),
                                          retry_after);
      }
    }
    return PrefetchErrorOnResponseReceived::kFailedNon2XX;
  }

  if (PrefetchServiceHTMLOnly() && head->mime_type != "text/html") {
    prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchFailedMIMENotSupported);
    return PrefetchErrorOnResponseReceived::kFailedMIMENotSupported;
  }
  return std::nullopt;
}

void PrefetchService::OnWillBeDestroyed(PrefetchContainer& prefetch_container) {
}

void PrefetchService::OnGotInitialEligibility(
    PrefetchContainer& prefetch_container,
    PreloadingEligibility eligibility) {}

void PrefetchService::OnDeterminedHead(PrefetchContainer& prefetch_container) {}

void PrefetchService::OnPrefetchCompletedOrFailed(
    PrefetchContainer& prefetch_container,
    const network::URLLoaderCompletionStatus& completion_status,
    const std::optional<int>& response_code) {
  TRACE_EVENT("loading", "PrefetchService::OnPrefetchCompletedOrFailed",
              "prefetch_url", prefetch_container.GetURL().spec(),
              "completion_status.error_code", completion_status.error_code);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(IsPrefetchContainerInActiveSet(prefetch_container));

  if (!UsePrefetchScheduler()) {
    active_prefetch_ = std::nullopt;
    Prefetch();
  } else {
    RemoveFromSchedulerAndProgressAsync(prefetch_container);
  }
}

void PrefetchService::CopyIsolatedCookies(
    const PrefetchServingHandle& serving_handle) {
  DCHECK(serving_handle);

  if (!serving_handle.GetCurrentNetworkContextToServe()) {
    // Not set in unit tests.
    return;
  }

  // We only need to copy cookies if the prefetch used an isolated network
  // context.
  if (!serving_handle.IsIsolatedNetworkContextRequiredToServe()) {
    return;
  }

  serving_handle.OnIsolatedCookieCopyStart();
  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  serving_handle.GetCurrentNetworkContextToServe()
      ->GetCookieManager()
      ->GetCookieList(
          serving_handle.GetCurrentURLToServe(), options,
          net::CookiePartitionKeyCollection::Todo(),
          base::BindOnce(&PrefetchService::OnGotIsolatedCookiesForCopy,
                         weak_method_factory_.GetWeakPtr(),
                         serving_handle.Clone()));
}

void PrefetchService::OnGotIsolatedCookiesForCopy(
    PrefetchServingHandle serving_handle,
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  serving_handle.OnIsolatedCookiesReadCompleteAndWriteStart();
  RecordPrefetchProxyPrefetchMainframeCookiesToCopy(cookie_list.size());

  if (cookie_list.empty()) {
    serving_handle.OnIsolatedCookieCopyComplete();
    return;
  }

  const auto current_url = serving_handle.GetCurrentURLToServe();

  base::RepeatingClosure barrier = base::BarrierClosure(
      cookie_list.size(),
      base::BindOnce(&OnIsolatedCookieCopyComplete, std::move(serving_handle)));

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  for (const net::CookieWithAccessResult& cookie : cookie_list) {
    browser_context_->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->SetCanonicalCookie(cookie.cookie, current_url, options,
                             base::BindOnce(&CookieSetHelper, barrier));
  }
}

// TODO(crbug.com/406754449): Inline this function when removing the feature
// flag.
bool PrefetchService::IsPrefetchContainerInActiveSet(
    const PrefetchContainer& prefetch_container) {
  if (!UsePrefetchScheduler()) {
    return active_prefetch_ == prefetch_container.key();
  } else {
    return scheduler_->IsInActiveSet(prefetch_container);
  }
}

void PrefetchService::DumpPrefetchesForDebug() const {
#if DCHECK_IS_ON()
  std::ostringstream ss;
  ss << "PrefetchService[" << this << "]:" << std::endl;

  ss << "Owned:" << std::endl;
  for (const auto& entry : owned_prefetches()) {
    ss << *entry.second << std::endl;
  }

  DVLOG(1) << ss.str();
#endif  // DCHECK_IS_ON()
}

std::pair<std::vector<PrefetchContainer*>,
          base::flat_map<PrefetchKey, PrefetchServableState>>
PrefetchService::CollectMatchCandidates(
    const PrefetchKey& key,
    bool is_nav_prerender,
    base::WeakPtr<PrefetchServingPageMetricsContainer>
        serving_page_metrics_container) {
  return CollectMatchCandidatesGeneric(
      owned_prefetches(), key, is_nav_prerender,
      std::move(serving_page_metrics_container));
}

PrefetchContainer* PrefetchService::FindPrefetchAheadOfPrerenderForMetrics(
    const PreloadPipelineInfo& pipeline_info) {
  for (const auto& it : owned_prefetches()) {
    auto& prefetch_container = it.second;
    if (prefetch_container->HasPreloadPipelineInfoForMetrics(pipeline_info)) {
      return prefetch_container.get();
    }
  }

  return nullptr;
}

base::WeakPtr<PrefetchContainer> PrefetchService::MatchUrl(
    const PrefetchKey& key) const {
  return no_vary_search::MatchUrl(key, owned_prefetches());
}

std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
PrefetchService::GetAllForUrlWithoutRefAndQueryForTesting(
    const PrefetchKey& key) const {
  return no_vary_search::GetAllForUrlWithoutRefAndQueryForTesting(
      key, owned_prefetches());
}

// static
void PrefetchService::SetServiceWorkerContextForTesting(
    ServiceWorkerContext* context) {
  g_service_worker_context_for_testing = context;
}

// static
void PrefetchService::SetHostNonUniqueFilterForTesting(
    bool (*filter)(std::string_view)) {
  g_host_non_unique_filter = filter;
}

// static
void PrefetchService::SetURLLoaderFactoryForTesting(
    network::SharedURLLoaderFactory* url_loader_factory) {
  g_url_loader_factory_for_testing = url_loader_factory;
}

// static
void PrefetchService::SetNetworkContextForProxyLookupForTesting(
    network::mojom::NetworkContext* network_context) {
  g_network_context_for_proxy_lookup_for_testing = network_context;
}

// static
void PrefetchService::SetInjectedEligibilityCheckForTesting(
    InjectedEligibilityCheckForTesting callback) {
  GetInjectedEligibilityCheckForTesting() =  // IN-TEST
      std::move(callback);
}

base::WeakPtr<PrefetchService> PrefetchService::GetWeakPtr() {
  return weak_method_factory_.GetWeakPtr();
}

void PrefetchService::EvictPrefetchesForBrowsingDataRemoval(
    const StoragePartition::StorageKeyMatcherFunction& storage_key_filter,
    PrefetchStatus status) {
  std::vector<base::WeakPtr<PrefetchContainer>> prefetches_to_reset;
  for (const auto& prefetch_iter : owned_prefetches()) {
    base::WeakPtr<PrefetchContainer> prefetch_container =
        prefetch_iter.second->GetWeakPtr();
    CHECK(prefetch_container);

    // If `referring_origin` is std::nullopt (e.g some browser-initiated
    // prefetch), use the origin of the prefetch URL itself, since we generally
    // handle no referring origin prefetches as a same-origin prefetch fashion.
    const url::Origin target_origin =
        prefetch_container->request().referring_origin().value_or(
            url::Origin::Create(prefetch_container->GetURL()));
    if (storage_key_filter.Run(
            blink::StorageKey::CreateFirstParty(target_origin))) {
      prefetch_container->SetPrefetchStatus(status);
      prefetches_to_reset.push_back(prefetch_container);
    }
  }

  if (!UsePrefetchScheduler()) {
    for (const auto& prefetch_container : prefetches_to_reset) {
      ResetPrefetchContainer(prefetch_container);
    }
  } else {
    ResetPrefetchContainersAndProgressAsync(std::move(prefetches_to_reset));
  }
}

}  // namespace content
