// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_service.h"

#include <optional>
#include <string_view>

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/variations/net/omnibox_autofocus_http_headers.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/browser/browser_context_impl.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_document_manager.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_handle_impl.h"
#include "content/browser/preloading/prefetch/prefetch_isolated_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_network_context_client.h"
#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_proxy_configurator.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_resource_request_utils.h"
#include "content/browser/preloading/prefetch/prefetch_scheduler.h"
#include "content/browser/preloading/prefetch/prefetch_servable_state.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/preloading_attempt_impl.h"
#include "content/browser/preloading/prerender/prerender_features.h"
#include "content/browser/preloading/proxy_lookup_client_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/prefetch_service_delegate.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/spare_render_process_host_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/loader/referrer_utils.h"
#include "third_party/blink/public/common/navigation/preloading_headers.h"
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
      // Eligible prefetches don't need a decoy.
      return false;
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

// Returns true if the prefetch is heldback, and set the holdback status
// correspondingly.
bool CheckAndSetPrefetchHoldbackStatus(PrefetchContainer& prefetch_container) {
  if (!prefetch_container.request().attempt()) {
    return false;
  }

  bool devtools_client_exist = [&] {
    // Currently DevTools only supports when the prefetch is initiated by
    // renderer.
    auto* renderer_initiator_info =
        prefetch_container.request().GetRendererInitiatorInfo();
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
    prefetch_container.request().attempt()->SetHoldbackStatus(
        PreloadingHoldbackStatus::kAllowed);
  } else if (prefetch_container.IsLikelyAheadOfPrerender()) {
    // 2. If PrefetchContainer is likely ahead of prerender, always set status
    // to kAllowed as it is likely used for prerender.
    //
    // Note that we don't use
    // `PrefetchContainer::request().holdback_status_override()` for this
    // purpose because it can't handle a prefetch that was not ahead of
    // prerender but another ahead of prerender one is migrated into it. We need
    // to update migration if we'd like to do it.
    prefetch_container.request().attempt()->SetHoldbackStatus(
        PreloadingHoldbackStatus::kAllowed);
  } else if (prefetch_container.request().holdback_status_override() !=
             PreloadingHoldbackStatus::kUnspecified) {
    // 3. If PrefetchContainer has custom overridden status, set that value.
    prefetch_container.request().attempt()->SetHoldbackStatus(
        prefetch_container.request().holdback_status_override());
  }

  if (prefetch_container.request().attempt()->ShouldHoldback()) {
    prefetch_container.SetLoadState(
        PrefetchContainer::LoadState::kFailedHeldback);
    prefetch_container.SetPrefetchStatus(PrefetchStatus::kPrefetchHeldback);
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
      scheduler_(std::make_unique<PrefetchScheduler>(this)) {}

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
  CHECK(prefetch_request);
  CHECK_EQ(prefetch_request->browser_context(), GetBrowserContext());

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

    switch (prefetch_container_old.GetMatchResolverAction().ToServableState()) {
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
      scheduler_->NotifyAttributeMightChangedAndProgressAsync(
          *prefetch_iter->second, /*should_progress=*/false);
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

  PrefetchServableState servable_state =
      prefetch_container->GetMatchResolverAction().ToServableState();
  return servable_state == PrefetchServableState::kNotServable;
}

// Parameter class used during eligibility check and `OnGotEligibility*` methods
// (`callback`).
struct PrefetchService::CheckEligibilityParams final {
  void Finish(PreloadingEligibility eligibility) && {
    // `callback_local` is needed to avoid use-after-move.
    auto callback_local = std::move(callback);
    std::move(callback_local).Run(std::move(*this), eligibility);
  }

  // Methods accessing `prefetch_container_internal`. These should be used
  // (instead of `prefetch_container_internal` directly) in order to decouple
  // the eligibility check logic from `PrefetchContainer` as much as possible.
  // These values are immutable throughout `PrefetchContainer` lifetime or at
  // least are not affected by `AddRedirectHop()` timing, and therefore:
  // - They are safe to call during crbug.com/432518638.
  // - We might further want to decouple these methods from
  //   `prefetch_container_internal` by storing these values in
  //   `CheckEligibilityParams`.

  bool IsAlive() const { return !!prefetch_container_internal; }

  // Returns if proxy is required for the next request.
  bool IsProxyRequired() const {
    return request().IsProxyRequiredForURL(url) &&
           !ShouldPrefetchBypassProxyForTestHost(url.GetHost());
  }

  // Note: this is the initial prefetch URL (for preserving the existing
  // behavior) and is different from `url` on redirect.
  std::string PrefetchUrlForTrace() const {
    return IsAlive() ? prefetch_container_internal->GetURL().spec() : "";
  }

  const PrefetchRequest& request() const {
    CHECK(IsAlive());
    return prefetch_container_internal->request();
  }

  PrefetchServiceWorkerState service_worker_state() const {
    CHECK(IsAlive());
    return prefetch_container_internal->service_worker_state();
  }

  bool is_isolated_network_context_required() const {
    return request().IsIsolatedNetworkContextRequired(url);
  }

  void MarkCrossSiteContaminated() {
    CHECK(IsAlive());
    prefetch_container_internal->MarkCrossSiteContaminated();
  }

  // Do not directly use this during the eligibility check, except for in
  // `OnGotEligibility*` (where the eligibility check is already finished).
  base::WeakPtr<PrefetchContainer> prefetch_container_internal;

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
      {.prefetch_container_internal = prefetch_container,
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
  if (!params.IsAlive()) {
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
  CHECK(params.IsAlive());

  TRACE_EVENT_END("loading",
                  params.request().preload_pipeline_info().GetTrack());
  TRACE_EVENT_BEGIN("loading", "PrefetchService::CheckEligibility",
                    params.request().preload_pipeline_info().GetTrack());

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
  CHECK(params.IsAlive());

  TRACE_EVENT_END("loading",
                  params.request().preload_pipeline_info().GetTrack());
  TRACE_EVENT_BEGIN("loading", "PrefetchService::CheckHasServiceWorker",
                    params.request().preload_pipeline_info().GetTrack());

  if (params.is_redirect) {
    switch (params.service_worker_state()) {
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
    switch (params.service_worker_state()) {
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
  if (auto* preloading_attempt =
          static_cast<PreloadingAttemptImpl*>(params.request().attempt())) {
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
  TRACE_EVENT("loading", "PrefetchService::OnGotServiceWorkerResult",
              "prefetch_url", params.PrefetchUrlForTrace());

  if (!params.IsAlive()) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  TRACE_EVENT_END("loading",
                  params.request().preload_pipeline_info().GetTrack());
  TRACE_EVENT_BEGIN("loading", "PrefetchService::OnGotServiceWorkerResult",
                    params.request().preload_pipeline_info().GetTrack());

  if (auto* preloading_attempt =
          static_cast<PreloadingAttemptImpl*>(params.request().attempt())) {
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
      // We still don't use ServiceWorker-ineligible prefetch results if there
      // is a controlling service worker at the time of navigation even if it
      // doesn't have fetch handlers. So we prevent prefetching here as well, to
      // avoid useless prefetches.
      std::move(params).Finish(
          PreloadingEligibility::kUserHasServiceWorkerNoFetchHandler);
      return;
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
      !params.is_isolated_network_context_required()) {
    std::move(params).Finish(
        PreloadingEligibility::kSameSiteCrossOriginPrefetchRequiredProxy);
    return;
  }
  // We do not need to check the cookies of prefetches that do not need an
  // isolated network context.
  if (!params.is_isolated_network_context_required()) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  StoragePartition* default_storage_partition =
      browser_context_->GetDefaultStoragePartition();
  CHECK(default_storage_partition);

  TRACE_EVENT_END("loading",
                  params.request().preload_pipeline_info().GetTrack());
  TRACE_EVENT_BEGIN("loading",
                    "PrefetchService::OnGotServiceWorkerResult check cookies",
                    params.request().preload_pipeline_info().GetTrack());

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
  TRACE_EVENT("loading", "PrefetchService::OnGotCookiesForEligibilityCheck",
              "prefetch_url", params.PrefetchUrlForTrace());

  if (!params.IsAlive()) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  TRACE_EVENT_END("loading",
                  params.request().preload_pipeline_info().GetTrack());
  TRACE_EVENT_BEGIN("loading",
                    "PrefetchService::OnGotCookiesForEligibilityCheck",
                    params.request().preload_pipeline_info().GetTrack());

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
        delegate_ && params.request().referring_origin().has_value() &&
        delegate_->IsContaminationExempt(
            params.request().referring_origin().value());
    if (!is_contamination_exempt) {
      params.MarkCrossSiteContaminated();
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
  // Same origin prefetches (which use the default network context and cannot
  // use the prefetch proxy) can use the existing proxy settings.
  // TODO(crbug.com/40231580): Copy proxy settings over to the isolated
  // network context for the prefetch in order to allow non-private cross origin
  // prefetches to be made using the existing proxy settings.
  if (!params.is_isolated_network_context_required()) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  TRACE_EVENT_END("loading",
                  params.request().preload_pipeline_info().GetTrack());
  TRACE_EVENT_BEGIN("loading", "PrefetchService::ProxyCheck",
                    params.request().preload_pipeline_info().GetTrack());

  // Start proxy check for this prefetch, and give ownership of the
  // |ProxyLookupClientImpl| to |prefetch_container|.
  // `url` is needed to avoid use-after-move.
  const GURL url = params.url;
  ProxyLookupClientImpl::CreateAndStart(
      url,
      net::NetworkAnonymizationKey::CreateSameSite(net::SchemefulSite(url)),
      base::BindOnce(&PrefetchService::OnGotProxyLookupResult,
                     weak_method_factory_.GetWeakPtr(), std::move(params)),
      g_network_context_for_proxy_lookup_for_testing
          ? g_network_context_for_proxy_lookup_for_testing
          : browser_context_->GetDefaultStoragePartition()
                ->GetNetworkContext());
}

void PrefetchService::OnGotProxyLookupResult(CheckEligibilityParams params,
                                             bool has_proxy) {
  TRACE_EVENT("loading", "PrefetchService::OnGotProxyLookupResult",
              "prefetch_url", params.PrefetchUrlForTrace());

  if (!params.IsAlive()) {
    std::move(params).Finish(PreloadingEligibility::kEligible);
    return;
  }

  TRACE_EVENT_END("loading",
                  params.request().preload_pipeline_info().GetTrack());
  TRACE_EVENT_BEGIN("loading", "PrefetchService::OnGotProxyLookupResult",
                    params.request().preload_pipeline_info().GetTrack());

  if (has_proxy) {
    std::move(params).Finish(PreloadingEligibility::kExistingProxy);
    return;
  }

  std::move(params).Finish(PreloadingEligibility::kEligible);
}

void PrefetchService::OnGotEligibilityForNonRedirect(
    CheckEligibilityParams params,
    PreloadingEligibility eligibility) {
  const auto prefetch_container = params.prefetch_container_internal;

  TRACE_EVENT("loading", "PrefetchService::OnGotEligibilityForNonRedirect",
              "prefetch_url", params.PrefetchUrlForTrace());

  if (!prefetch_container) {
    return;
  }

  TRACE_EVENT_END("loading",
                  params.request().preload_pipeline_info().GetTrack());
  TRACE_EVENT_BEGIN("loading",
                    "PrefetchService::OnGotEligibilityForNonRedirect",
                    params.request().preload_pipeline_info().GetTrack());

  const bool eligible = eligibility == PreloadingEligibility::kEligible;
  const bool is_decoy =
      params.IsProxyRequired() &&
      ShouldConsiderDecoyRequestForStatus(eligibility) &&
      PrefetchServiceSendDecoyRequestForIneligblePrefetch(
          delegate_ ? delegate_->DisableDecoysBasedOnUserSettings() : false);
  // The prefetch decoy is pushed onto the queue and the network request will be
  // dispatched, but the response will not be used. Thus it is eligible but a
  // failure.
  prefetch_container->SetIsDecoy(is_decoy);
  prefetch_container->OnEligibilityCheckComplete(eligibility);

  if (!eligible && !is_decoy) {
    DVLOG(1)
        << *prefetch_container
        << ": not prefetched (not eligible nor decoy. PreloadingEligibility="
        << static_cast<int>(eligibility) << ")";
    return;
  }

  ScheduleAndProgress(std::move(prefetch_container));
}

void PrefetchService::OnGotEligibilityForRedirect(
    net::RedirectInfo redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head,
    CheckEligibilityParams params,
    PreloadingEligibility eligibility) {
  const auto prefetch_container = params.prefetch_container_internal;

  TRACE_EVENT("loading", "PrefetchService::OnGotEligibilityForRedirect",
              "prefetch_url", params.PrefetchUrlForTrace());

  if (!prefetch_container) {
    return;
  }

  TRACE_EVENT_END("loading",
                  params.request().preload_pipeline_info().GetTrack());
  TRACE_EVENT_BEGIN("loading", "PrefetchService::OnGotEligibilityForRedirect",
                    params.request().preload_pipeline_info().GetTrack());

  // Returns `false` if `OnGotEligibilityForRedirect()` should be early-returned
  // because the prefetch was already terminated during the eligiblity check.
  const auto check_streaming_loader = [&]() {
    // TODO(crbug.com/396133768): Consider setting appropriate PrefetchStatus.
    auto streaming_url_loader = prefetch_container->GetStreamingURLLoader();
    if (streaming_url_loader) {
      return true;
    }

    // TODO(crbug.com/400761083): Use
    // `ResetPrefetchContainerAndProgressAsync()` instead.
    RemoveFromSchedulerAndProgressAsync(*prefetch_container);

    return false;
  };

  if (!check_streaming_loader()) {
    // TODO(crbug.com/400761083): Turn this into `CHECK_EQ()`.
    DUMP_WILL_BE_CHECK_EQ(prefetch_container->GetLoadState(),
                          PrefetchContainer::LoadState::kFailed);
    return;
  }

  const bool eligible = eligibility == PreloadingEligibility::kEligible;
  RecordRedirectResult(eligible
                           ? PrefetchRedirectResult::kSuccessRedirectFollowed
                           : PrefetchRedirectResult::kFailedIneligible);

  // If the redirect is ineligible, the prefetch may change into a decoy.
  const bool is_decoy =
      params.IsProxyRequired() &&
      ShouldConsiderDecoyRequestForStatus(eligibility) &&
      PrefetchServiceSendDecoyRequestForIneligblePrefetch(
          delegate_ ? delegate_->DisableDecoysBasedOnUserSettings() : false);
  prefetch_container->SetIsDecoy(prefetch_container->IsDecoy() || is_decoy);

  // Inform the prefetch container of the result of the eligibility check
  prefetch_container->OnEligibilityCheckComplete(eligibility);

  auto streaming_url_loader = prefetch_container->GetStreamingURLLoader();
  CHECK(streaming_url_loader);

  // If the redirect is not eligible and the prefetch is not a decoy, then stop
  // the prefetch.
  if (!eligible && !prefetch_container->IsDecoy()) {
    CHECK(scheduler_->IsInActiveSet(*prefetch_container));

    // Remove first as it requires that `PrefetchContainer` is available.
    RemoveFromSchedulerAndProgressAsync(*prefetch_container);

    streaming_url_loader->HandleRedirect(
        PrefetchRedirectStatus::kFail, redirect_info, std::move(redirect_head),
        /*update_headers_params=*/{});

    // TODO(https://crbug.com/400761083): Use
    // `ResetPrefetchContainerAndProgressAsync()` instead.
    return;
  }

  auto [updates_for_resource_request, updates_for_follow_redirect] =
      prefetch_container->PrepareUpdateHeaders(redirect_info.new_url);

  prefetch_container->UpdateResourceRequest(
      redirect_info, std::move(updates_for_resource_request));

  prefetch_container->NotifyPrefetchRequestWillBeSent(&redirect_head);

  // If the redirect requires a change in network contexts, then stop the
  // current streaming URL loader and start a new streaming URL loader for the
  // redirect URL.
  if (params.is_isolated_network_context_required() !=
      prefetch_container
          ->IsIsolatedNetworkContextRequiredForPreviousRedirectHop()) {
    streaming_url_loader->HandleRedirect(
        PrefetchRedirectStatus::kSwitchNetworkContext, redirect_info,
        std::move(redirect_head), /*update_headers_params=*/{});
    // The new ResponseReader is associated with the new streaming URL loader at
    // the PrefetchStreamingURLLoader constructor.
    SendPrefetchRequest(*prefetch_container);

    return;
  }

  // Otherwise, follow the redirect in the same streaming URL loader.
  streaming_url_loader->HandleRedirect(PrefetchRedirectStatus::kFollow,
                                       redirect_info, std::move(redirect_head),
                                       std::move(updates_for_follow_redirect));
  // Associate the new ResponseReader with the current streaming URL loader.
  streaming_url_loader->SetResponseReader(
      prefetch_container->GetResponseReaderForCurrentPrefetch());
}

void PrefetchService::OnPrefetchTimeout(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  prefetch_container->SetPrefetchStatus(PrefetchStatus::kPrefetchIsStale);
  ResetPrefetchContainerAndProgressAsync(std::move(prefetch_container));
}

void PrefetchService::MayReleasePrefetch(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  if (!prefetch_container) {
    return;
  }

  if (!owned_prefetches().contains(prefetch_container->key())) {
    return;
  }

  ResetPrefetchContainerAndProgressAsync(std::move(prefetch_container));
}

void PrefetchService::ResetPrefetchContainer(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    bool should_progress) {
  CHECK(prefetch_container);

  // Remove before calling `PrefetchContainer::dtor()` as `PrefetchScheduler`
  // manages them with weak pointers.
  scheduler_->RemoveAndProgressAsync(*prefetch_container, should_progress);

  auto it = owned_prefetches().find(prefetch_container->key());
  CHECK(it != owned_prefetches().end());
  CHECK_EQ(it->second.get(), prefetch_container.get());
  owned_prefetches_.erase(it);
}

void PrefetchService::ScheduleAndProgress(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  CHECK(prefetch_container);

  scheduler_->PushAndProgress(*prefetch_container);
}

void PrefetchService::ScheduleAndProgressAsync(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  CHECK(prefetch_container);

  scheduler_->PushAndProgressAsync(*prefetch_container);

  // `PrefetchScheduler::Progress()` will be called asynchronously.
}

void PrefetchService::ResetPrefetchContainerAndProgressAsync(
    base::WeakPtr<PrefetchContainer> prefetch_container) {
  ResetPrefetchContainer(std::move(prefetch_container));

  // `PrefetchScheduler::Progress()` will be called asynchronously.
}

void PrefetchService::ResetPrefetchContainersAndProgressAsync(
    std::vector<base::WeakPtr<PrefetchContainer>> prefetch_containers) {
  for (auto& prefetch_container : prefetch_containers) {
    ResetPrefetchContainer(std::move(prefetch_container));
  }

  // `PrefetchScheduler::Progress()` will be called asynchronously.
}

void PrefetchService::RemoveFromSchedulerAndProgressAsync(
    const PrefetchContainer& prefetch_container) {
  scheduler_->RemoveAndProgressAsync(prefetch_container);

  // `PrefetchScheduler::Progress()` will be called asynchronously.
}

void PrefetchService::OnCandidatesUpdated() {
  // Call `PrefetchScheduler::Progress()` by a historical reason.
  //
  // Prior to `PrefetchScheduler`, we used a naive FIFO queue to manage pending
  // prefetches. This sometimes caused livelocks where an active prefetch failed
  // to deque after loading completed, causing the queue to stall.
  //
  // `PrefetchScheduler` ensures that modifying `PrefetchQueue` triggers
  // `PrefetchScheduler::Progress()` eventually. So, we believe that this
  // explicit `Progress()` call is not necessary. But we keep it because 1. It's
  // safe (as it's not reentrancy) and noop if not necessary. 2. We should
  // another experiment to remove the call as we are using `PrefetchScheduler`
  // in some experiments.
  //
  // TODO(crbug.com/406754449): Consider to remove it.
  scheduler_->Progress();
}

void PrefetchService::AddRecentUnmatchedNavigatedKeysForMetrics(
    const PrefetchKey& navigated_key) {
  recent_unmatched_navigated_keys_for_metrics_.Put(navigated_key,
                                                   base::TimeTicks::Now());
}

void PrefetchService::PrepareProgress(base::PassKey<PrefetchScheduler>) {
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

void PrefetchService::EvictPrefetch(base::PassKey<PrefetchScheduler>,
                                    PrefetchContainer& prefetch_container) {
  prefetch_container.SetPrefetchStatus(
      PrefetchStatus::kPrefetchEvictedForNewerPrefetch);
  ResetPrefetchContainer(prefetch_container.GetWeakPtr());
}

bool PrefetchService::StartSinglePrefetch(
    base::PassKey<PrefetchScheduler>,
    PrefetchContainer& prefetch_container) {
  return StartSinglePrefetch(prefetch_container);
}

bool PrefetchService::StartSinglePrefetchForTesting(
    PrefetchContainer& prefetch_container) {
  return StartSinglePrefetch(prefetch_container);
}

bool PrefetchService::StartSinglePrefetch(
    PrefetchContainer& prefetch_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(prefetch_container.GetLoadState(),
           PrefetchContainer::LoadState::kEligible);
  TRACE_EVENT("loading", "PrefetchService::StartSinglePrefetch",
              prefetch_container.request().preload_pipeline_info().GetFlow());

  // Do not prefetch for a Holdback control group. Called after the checks in
  // `PopNextPrefetchContainer` because we want to compare against the
  // prefetches that would have been dispatched.
  if (CheckAndSetPrefetchHoldbackStatus(prefetch_container)) {
    DVLOG(1) << prefetch_container
             << ": not prefetched (holdback control group)";
    return false;
  }

  prefetch_container.OnPrefetchStarted();

  // Checks if the `PrefetchContainer` has a specific TTL (Time-to-Live)
  // configured. If a TTL is configured, the prefetch container will be eligible
  // for removal after the TTL expires. Otherwise, it will remain alive
  // indefinitely.
  //
  // The default TTL is determined by
  // `PrefetchContainerDefaultTtlInPrefetchService()`, which may return a zero
  // or negative value, indicating an indefinite TTL.
  prefetch_container.StartTimeoutTimerIfNeeded(base::BindOnce(
      &PrefetchService::OnPrefetchTimeout, weak_method_factory_.GetWeakPtr(),
      prefetch_container.GetWeakPtr()));

  if (!prefetch_container.IsDecoy()) {
    // The status is updated to be successful or failed when it finishes.
    prefetch_container.SetPrefetchStatus(
        PrefetchStatus::kPrefetchNotFinishedInTime);
  }

  prefetch_container.MakeInitialResourceRequest();

  prefetch_container.NotifyPrefetchRequestWillBeSent(
      /*redirect_head=*/nullptr);

  SendPrefetchRequest(prefetch_container);

  PrefetchDocumentManager* prefetch_document_manager = nullptr;
  if (auto* renderer_initiator_info =
          prefetch_container.request().GetRendererInitiatorInfo()) {
    prefetch_document_manager =
        renderer_initiator_info->prefetch_document_manager();
  }

  if (prefetch_container.request()
          .prefetch_type()
          .IsProxyRequiredWhenCrossOrigin() &&
      !prefetch_container.IsDecoy() &&
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
    PrefetchContainer& prefetch_container) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT("loading", "PrefetchService::SendPrefetchRequest",
              prefetch_container.request().preload_pipeline_info().GetFlow());

  base::WeakPtr<PrefetchContainer> weak_prefetch_container =
      prefetch_container.GetWeakPtr();
  auto streaming_loader = PrefetchStreamingURLLoader::CreateAndStart(
      GetURLLoaderFactoryForCurrentPrefetch(prefetch_container),
      *prefetch_container.GetResourceRequest(),
      kNavigationalPrefetchTrafficAnnotation, PrefetchTimeoutDuration(),
      base::BindOnce(&PrefetchService::OnPrefetchResponseStarted,
                     base::Unretained(this), weak_prefetch_container),
      base::BindRepeating(&PrefetchService::OnPrefetchRedirect,
                          base::Unretained(this), weak_prefetch_container),
      prefetch_container.GetResponseReaderForCurrentPrefetch(),
      prefetch_container.service_worker_state(), browser_context_,
      base::BindOnce(&PrefetchContainer::OnServiceWorkerStateDetermined,
                     weak_prefetch_container),
      prefetch_container.request().preload_pipeline_info().GetFlow());
  prefetch_container.SetStreamingURLLoader(std::move(streaming_loader));

  DVLOG(1) << prefetch_container << ": PrefetchStreamingURLLoader is created.";
}

void PrefetchService::MaybeSetPrefetchMatchMissedTimeForMetrics(
    PrefetchContainer& prefetch_container) const {
  // Check if the prefetch could've been matched to a recently navigated URL.
  // We will log the event to UMA in order to understand if a potential race
  // condition has occurred.
  // TODO(crbug.com/433478563): Introduce another variant of
  // `no_vary_search::MatchUrl` that doesn't require a map.
  std::map<PrefetchKey, base::WeakPtr<PrefetchContainer>> candidates;
  candidates[prefetch_container.key()] = prefetch_container.GetWeakPtr();
  for (auto& it : recent_unmatched_navigated_keys_for_metrics_) {
    bool urls_match = !!no_vary_search::MatchUrl(it.first, candidates);
    if (urls_match) {
      prefetch_container.SetPrefetchMatchMissedTimeForMetrics(it.second);
      break;
    }
  }
}

namespace {

// Enable Zstd for cross-site prefetch (crbug.com/444393104).
BASE_FEATURE(kZstdForCrossSiteSpeculationRulesPrefetch,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allow the variations header to be treated as CORS exempted for cross-site
// prefetch (crbug.com/444264052).
BASE_FEATURE(kVariationsHeaderForCrossSiteSpeculationRulesPrefetch,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace

mojo::Remote<network::mojom::NetworkContext>
PrefetchService::CreateIsolatedNetworkContext(
    bool is_proxy_required_when_cross_origin) {
  auto context_params = network::mojom::NetworkContextParams::New();
  context_params->file_paths = network::mojom::NetworkContextFilePaths::New();

  // These should be synced with
  // `SystemNetworkContextManager::ConfigureDefaultNetworkContextParams()`.
  // TODO(crbug.com/444335342): Unify NetworkContextParams setup with other
  // places.
  context_params->enable_zstd =
      base::FeatureList::IsEnabled(kZstdForCrossSiteSpeculationRulesPrefetch);
  context_params->user_agent = embedder_support::GetUserAgent();

  // The verifier created here does not have the same parameters as used in the
  // profile (where additional parameters are added in
  // chrome/browser/net/profile_network_context_service.h
  // ProfileNetworkContextService::ConfigureNetworkContextParamsInternal, as
  // well as updates in ProfileNetworkContextService::UpdateCertificatePolicy).
  //
  // Currently this does not cause problems as additional parameters only ensure
  // more requests validate, so the only harm is that prefetch requests will
  // fail and then later succeed when they are actually fetched. In the future
  // when additional parameters can cause validations to fail, this will cause
  // problems.
  //
  // TODO(crbug.com/40928765): figure out how to get this verifier in sync with
  // the profile verifier.
  context_params->cert_verifier_params = GetCertVerifierParams(
      cert_verifier::mojom::CertVerifierCreationParams::New());
  context_params->cors_exempt_header_list = {blink::kPurposeHeaderName};
  if (base::FeatureList::IsEnabled(
          kVariationsHeaderForCrossSiteSpeculationRulesPrefetch)) {
    variations::UpdateCorsExemptHeaderForVariations(context_params.get());
    variations::UpdateCorsExemptHeaderForOmniboxAutofocus(context_params.get());
  }
  GetContentClient()->browser()->UpdateCorsExemptHeaderForPrefetch(
      context_params.get());

  context_params->cookie_manager_params =
      network::mojom::CookieManagerParams::New();

  if (delegate_) {
    context_params->accept_language = delegate_->GetAcceptLanguageHeader();
  }

  context_params->http_cache_enabled = true;
  CHECK(!context_params->file_paths->http_cache_directory);

  if (is_proxy_required_when_cross_origin) {
    CHECK(prefetch_proxy_configurator_);

    context_params->initial_custom_proxy_config =
        prefetch_proxy_configurator_->CreateCustomProxyConfig();
    context_params->custom_proxy_connection_observer_remote =
        prefetch_proxy_configurator_->NewProxyConnectionObserverRemote();

    // Register a client config receiver so that updates to the set of proxy
    // hosts or proxy headers will be updated.
    mojo::Remote<network::mojom::CustomProxyConfigClient> config_client;
    context_params->custom_proxy_config_client_receiver =
        config_client.BindNewPipeAndPassReceiver();
    prefetch_proxy_configurator_->AddCustomProxyConfigClient(
        std::move(config_client), base::DoNothing());
  }

  // Explicitly disallow network service features which could cause a privacy
  // leak.
  context_params->enable_certificate_reporting = false;
  context_params->enable_domain_reliability = false;

  mojo::Remote<network::mojom::NetworkContext> network_context;
  CreateNetworkContextInNetworkService(
      network_context.BindNewPipeAndPassReceiver(), std::move(context_params));

  if (is_proxy_required_when_cross_origin) {
    // Configure a context client to ensure Web Reports and other privacy leak
    // surfaces won't be enabled.
    mojo::PendingRemote<network::mojom::NetworkContextClient> client_remote;
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<PrefetchNetworkContextClient>(),
        client_remote.InitWithNewPipeAndPassReceiver());
    network_context->SetClient(std::move(client_remote));
  }
  return network_context;
}

mojo::Remote<network::mojom::NetworkContext>
PrefetchService::CreateIsolatedNetworkContextForTesting(  // IN-TEST
    bool is_proxy_required_when_cross_origin) {
  return CreateIsolatedNetworkContext(is_proxy_required_when_cross_origin);
}

scoped_refptr<network::SharedURLLoaderFactory>
PrefetchService::GetURLLoaderFactoryForCurrentPrefetch(
    PrefetchContainer& prefetch_container) {
  if (g_url_loader_factory_for_testing) {
    return base::WrapRefCounted(g_url_loader_factory_for_testing);
  }

  if (!prefetch_container
           .IsIsolatedNetworkContextRequiredForCurrentPrefetch()) {
    return prefetch_container
        .GetOrCreateDefaultNetworkContextURLLoaderFactory();
  }

  if (PrefetchIsolatedNetworkContext* isolated_network_context =
          prefetch_container.GetIsolatedNetworkContext()) {
    return isolated_network_context->GetURLLoaderFactory();
  }

  const bool is_proxy_required_when_cross_origin =
      prefetch_container.request()
          .prefetch_type()
          .IsProxyRequiredWhenCrossOrigin() &&
      !prefetch_container.request()
           .prefetch_type()
           .IsProxyBypassedForTesting();  // IN-TEST

  return prefetch_container
      .CreateIsolatedNetworkContext(
          CreateIsolatedNetworkContext(is_proxy_required_when_cross_origin))
      ->GetURLLoaderFactory();
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

  CHECK(scheduler_->IsInActiveSet(*prefetch_container));

  std::optional<PrefetchRedirectResult> failure;
  if (redirect_info.new_method != "GET") {
    failure = PrefetchRedirectResult::kFailedInvalidMethod;
  } else if (!redirect_head->headers ||
             redirect_head->headers->response_code() < 300 ||
             redirect_head->headers->response_code() >= 400) {
    failure = PrefetchRedirectResult::kFailedInvalidResponseCode;
  } else if (!net::SchemefulSite::IsSameSite(
                 prefetch_container->GetCurrentURL(), redirect_info.new_url) &&
             !IsReferrerPolicySufficientlyStrict(
                 blink::ReferrerUtils::NetToMojoReferrerPolicy(
                     redirect_info.new_referrer_policy))) {
    // The new referrer policy is not sufficiently strict to allow cross-site
    // redirects.
    failure = PrefetchRedirectResult::kFailedInsufficientReferrerPolicy;
  } else if (prefetch_container->request().GetBrowserInitiatorInfo() &&
             !content::GetContentClient()
                  ->browser()
                  ->ShouldAllowPrefetchRedirection(
                      *prefetch_container->request().browser_context(),
                      redirect_info.new_url,
                      prefetch_container->request()
                          .GetBrowserInitiatorInfo()
                          ->embedder_histogram_suffix())) {
    // TODO(crbug.com/413259638): If a finer granularity of metrics is
    // required, introduce a new type of `PrefetchRedirectResult`.
    failure = PrefetchRedirectResult::kFailedIneligible;
  }

  if (failure) {
    CHECK(scheduler_->IsInActiveSet(*prefetch_container));

    RecordRedirectResult(*failure);

    prefetch_container->SetPrefetchStatus(
        PrefetchStatus::kPrefetchFailedInvalidRedirect);

    // Remove first as it requires that `PrefetchContainer` is available.
    RemoveFromSchedulerAndProgressAsync(*prefetch_container);

    if (auto streaming_url_loader =
            prefetch_container->GetStreamingURLLoader()) {
      streaming_url_loader->HandleRedirect(
          PrefetchRedirectStatus::kFail, redirect_info,
          std::move(redirect_head), /*update_headers_params=*/{});
    }

    // TODO(crbug.com/400761083): Use
    // `ResetPrefetchContainerAndProgressAsync()` instead.
    return;
  }

  prefetch_container->AddRedirectHop(redirect_info.new_url);

  auto params = CheckEligibilityParams(
      {.prefetch_container_internal = prefetch_container,
       .url = redirect_info.new_url,
       .is_redirect = true,
       .callback = base::BindOnce(&PrefetchService::OnGotEligibilityForRedirect,
                                  weak_method_factory_.GetWeakPtr(),
                                  redirect_info, std::move(redirect_head))});

  RecordRedirectNetworkContextTransition(
      prefetch_container
          ->IsIsolatedNetworkContextRequiredForPreviousRedirectHop(),
      prefetch_container->request().IsIsolatedNetworkContextRequired(
          redirect_info.new_url));

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

void PrefetchService::OnWillBeDestroyed(
    const PrefetchContainer& prefetch_container) {}

void PrefetchService::OnGotInitialEligibility(
    const PrefetchContainer& prefetch_container,
    PreloadingEligibility eligibility) {}

void PrefetchService::OnDeterminedHead(
    const PrefetchContainer& prefetch_container) {}

void PrefetchService::OnPrefetchCompletedOrFailed(
    const PrefetchContainer& prefetch_container,
    const network::URLLoaderCompletionStatus& completion_status,
    const std::optional<int>& response_code) {
  TRACE_EVENT("loading", "PrefetchService::OnPrefetchCompletedOrFailed",
              "prefetch_url", prefetch_container.GetURL().spec(),
              "completion_status.error_code", completion_status.error_code);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(scheduler_->IsInActiveSet(prefetch_container));

  RemoveFromSchedulerAndProgressAsync(prefetch_container);
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
        serving_page_metrics_container,
    const PrefetchKey* key_ahead_of_prerender,
    PrefetchPotentialCandidateCollectResult*
        collect_result_ahead_of_prerender) {
  return CollectMatchCandidatesGeneric(
      owned_prefetches(), key, is_nav_prerender,
      std::move(serving_page_metrics_container), key_ahead_of_prerender,
      collect_result_ahead_of_prerender);
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

  ResetPrefetchContainersAndProgressAsync(std::move(prefetches_to_reset));
}

}  // namespace content
