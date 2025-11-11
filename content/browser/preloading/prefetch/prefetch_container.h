// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/browser/preloading/preload_pipeline_info_impl.h"
#include "content/browser/preloading/preload_serving_metrics.h"
#include "content/common/content_export.h"
#include "content/public/browser/preloading.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "url/gurl.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace network::mojom {
class CookieManager;
}  // namespace network::mojom

namespace url {
class Origin;
}  // namespace url

namespace content {

class PrefetchKey;
class PrefetchMatchResolverAction;
class PrefetchNetworkContext;
class PrefetchRequest;
class PrefetchResponseReader;
class PrefetchService;
class PrefetchServingHandle;
class PrefetchServingPageMetricsContainer;
class PrefetchSingleRedirectHop;
class PrefetchStreamingURLLoader;
class ProxyLookupClientImpl;
enum class PrefetchPotentialCandidateServingResult;
enum class PrefetchProbeResult;
enum class PrefetchServableState;

// Holds the relevant size information of the prefetched response. The struct is
// installed onto `PrefetchContainer`, and gets passed into
// `PrefetchFromStringURLLoader` to notify the associated `URLLoaderClient` of
// the actual size of the response, as `PrefetchFromStringURLLoader` is not
// aware of the prefetched request.
struct PrefetchResponseSizes {
  int64_t encoded_data_length;
  int64_t encoded_body_length;
  int64_t decoded_body_length;
};

// The state enum of the current prefetch, to replace `PrefetchStatus`.
// https://crbug.com/1494771
// Design doc for PrefetchContainer state transitions:
// https://docs.google.com/document/d/1dK4mAVoRrgTVTGdewthI_hA8AHirgXW8k6BmpK9gnBE/edit?usp=sharing
//
// Note that this is decoupled from `PrefetchContainer` to allow forward
// declaration to prevent circular include.
enum class PrefetchContainerLoadState {
  // --- Phase 1. [Initial state]
  kNotStarted,

  // --- Phase 2. The eligibility check for the initial request has completed
  // and `PreloadingAttempt::SetEligibility()` has been called.

  // Found eligible.
  kEligible,

  // [Final state] Found ineligible. `redirect_chain_[0].eligibility_`
  // contains the reason for being ineligible.
  kFailedIneligible,

  // --- Phase 3. PrefetchService::StartSinglePrefetch() has been called and
  // the holdback check has completed.

  // Not heldback:
  //
  // On these states, refer to `PrefetchResponseReader`s for detailed
  // prefetching state and servability.
  //
  // - `kStarted`: Prefetch is started.
  // - `kDeterminedHead` or `kFailedDeterminedHead`:
  //   `PrefetchContainer::OnDeterminedHead()` is called.
  //   `Observer::OnDeterminedHead()` is called after transitioning to this
  //   state. They will eventually transition to `kCompleted` or `kFailed`,
  //   respectively (except for the cases where no state transitions occur,
  //   which should be fixed by https://crbug.com/400761083).
  //   TODO(https://crbug.com/400761083): Probably we should make these
  //   `PrefetchContainer::LoadState`s directly correspond to
  //   `PrefetchResponseReader::LoadState`s. One scenario where currently
  //   these two `LoadState`s temporarily mismatch is: when
  //   `PrefetchResponseReader::LoadState` transitions directly from
  //   `kStarted` to `kFailed`, `PrefetchContainer::LoadState` transitions to
  //   `kFailedDeterminedHead` and then immediately to `kFailed`, in order to
  //   align `PrefetchContainer::LoadState` and
  //   `PrefetchContainer::Observer` calls. Revisit this later.
  // - [Final state] `kCompleted` or `kFailed`:
  //   `PrefetchContainer::OnPrefetchComplete()` is called, and its
  //   `PrefetchResponseReader::LoadState` is `kCompleted` or `kFailed`,
  //   respectively.
  //   `Observer::OnPrefetchCompletedOrFailed()` is called after transitioning
  //   to this state.
  //
  // TODO(https://crbug.com/432518638): Make more strict association with
  // `PrefetchContainer::LoadState` and `PrefetchResponseReader::LoadState`
  // and verify it by adding CHECK()s.
  //
  // Also, refer to `request().attempt()` for triggering outcome and failure
  // reasons for metrics.
  // `PreloadingAttempt::SetFailureReason()` can be only called on this state.
  // Note that these states of `request().attempt()` don't directly affect
  // `PrefetchResponseReader`'s servability.
  // (e.g. `PrefetchResponseReader::GetServableState()` can be still
  // `kServable` even if `request().attempt()` has a failure).
  kStarted,
  kDeterminedHead,
  kFailedDeterminedHead,
  kCompleted,
  kFailed,

  // [Final state] Heldback due to `PreloadingAttempt::ShouldHoldback()`.
  kFailedHeldback,
};

// This class contains the state for a request to prefetch a specific URL.
//
// A `PrefetchContainer` can have multiple
// `PrefetchSingleRedirectHop`s and `PrefetchStreamingURLLoader`s to
// support redirects. Each `PrefetchSingleRedirectHop` in
// `redirect_chain_` corresponds to a single redirect hop, while a single
// `PrefetchStreamingURLLoader` can receive multiple redirect hops unless
// network context switching is needed.
//
// For example:
//
// |PrefetchStreamingURLLoader A-----| |PrefetchStreamingURLLoader B ---------|
// HandleRedirect  - HandleRedirect  - HandleRedirect  - ReceiveResponse-Finish
// |S.RedirectHop0-| |S.RedirectHop1-| |S.RedirectHop2-| |S.RedirectHop3------|
//
// While prefetching (see methods named like "ForCurrentPrefetch" or
// "ToPrefetch"), `PrefetchSingleRedirectHop`es and
// `PrefetchStreamingURLLoader`s (among other members) are added and filled. The
// steps for creating these objects and associating with each other span
// multiple classes/methods:
//
// 1. A new `PrefetchSingleRedirectHop` and thus a new
// `PrefetchResponseReader` is created and added to `redirect_chain_`.
// This is done either in:
// - `PrefetchContainer` constructor [for an initial request], or
// - `AddRedirectHop()` [for a redirect].
//
// 2. The new `PrefetchResponseReader` (created at Step 1, referenced as
// `GetResponseReaderForCurrentPrefetch()`) is associated with the
// `PrefetchStreamingURLLoader` to be used.
// This is done either in (see the indirect call sites of
// `PrefetchStreamingURLLoader::SetResponseReader()`):
// - `PrefetchService::StartSinglePrefetch()` [initial request] or
// - `PrefetchService::OnGotEligibilityForRedirect()` [redirect].
// A new `PrefetchStreamingURLLoader` is also created if needed in
// `PrefetchService::MakePrefetchRequest()`.
class CONTENT_EXPORT PrefetchContainer {
 public:
  // In non-test, `PrefetchContainer` should be only created and owned by
  // `PrefetchService`.
  static std::unique_ptr<PrefetchContainer> Create(
      base::PassKey<PrefetchService>,
      std::unique_ptr<const PrefetchRequest> request);
  static std::unique_ptr<PrefetchContainer> CreateForTesting(
      std::unique_ptr<const PrefetchRequest> request);

  // Use `Create*()` above.
  PrefetchContainer(base::PassKey<PrefetchContainer>,
                    std::unique_ptr<const PrefetchRequest> request);

  ~PrefetchContainer();

  PrefetchContainer(const PrefetchContainer&) = delete;
  PrefetchContainer& operator=(const PrefetchContainer&) = delete;

  // Observer interface to listen to lifecycle events of `PrefetchContainer`.
  //
  // Each callback is called at most once in the lifecycle of a container.
  //
  // Be careful about using this. This is designed only for
  // `PrefetchMatchResolver` and some other prefetch-internal classes.
  class Observer : public base::CheckedObserver {
   public:
    // Called at the head of dtor.
    //
    // TODO(crbug.com/356314759): Update the description to "Called just
    // before dtor is called."
    virtual void OnWillBeDestroyed(PrefetchContainer& prefetch_container) = 0;
    // Called when initial eligibility is got.
    virtual void OnGotInitialEligibility(PrefetchContainer& prefetch_container,
                                         PreloadingEligibility eligibility) = 0;
    // Called if non-redirect header of prefetch response is determined, i.e.
    // successfully received or fetch requests including redirects failed.
    // Callers can check success/failure by `GetNonRedirectHead()`.
    virtual void OnDeterminedHead(PrefetchContainer& prefetch_container) = 0;
    // Called when load of prefetch completed or failed.
    virtual void OnPrefetchCompletedOrFailed(
        PrefetchContainer& prefetch_container,
        const network::URLLoaderCompletionStatus& completion_status,
        const std::optional<int>& response_code) = 0;
  };

  void OnWillBeDestroyed();

  const PrefetchKey& key() const;

  // The initial URL that was requested to be prefetched.
  const GURL& GetURL() const;

  // The current URL being fetched.
  GURL GetCurrentURL() const;

  // The previous URL, if this has been redirected. Invalid to call otherwise.
  GURL GetPreviousURL() const;

  // Whether or not an isolated network context is required to the next
  // prefetch.
  bool IsIsolatedNetworkContextRequiredForCurrentPrefetch() const;

  // Whether or not an isolated network context is required for the previous
  // redirect hop of the given url.
  bool IsIsolatedNetworkContextRequiredForPreviousRedirectHop() const;

  base::WeakPtr<PrefetchResponseReader> GetResponseReaderForCurrentPrefetch();

  // Whether or not the prefetch proxy would be required to fetch the given url
  // based on |prefetch_type_|.
  bool IsProxyRequiredForURL(const GURL& url) const;

  const network::ResourceRequest* GetResourceRequest() const {
    return resource_request_.get();
  }
  void MakeResourceRequest();

  // Updates |referrer_| after a redirect.
  void UpdateReferrer(
      const GURL& new_referrer_url,
      const network::mojom::ReferrerPolicy& new_referrer_policy);

  // Equivalent to `request().no_vary_search_hint()`.
  // Exposed for `PrefetchMatchResolver`.
  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint() const;

  base::WeakPtr<PrefetchContainer> GetWeakPtr() {
    return weak_method_factory_.GetWeakPtr();
  }

  // Sets the time that the latest earlier prefetch unmatch happened that this
  // prefetch could've been served to. Please see
  // `time_prefetch_match_missed_` for more details.
  void SetPrefetchMatchMissedTimeForMetrics(base::TimeTicks time);

  // The status of the current prefetch. Note that |HasPrefetchStatus| will be
  // initially false until |SetPrefetchStatus| is called. |SetPrefetchStatus|
  // also sets |request().attempt()| PreloadingTriggeringOutcome and
  // PreloadingFailureReason. It is only safe to call after
  // `OnEligibilityCheckComplete`.
  void SetPrefetchStatus(PrefetchStatus prefetch_status);
  bool HasPrefetchStatus() const { return prefetch_status_.has_value(); }
  PrefetchStatus GetPrefetchStatus() const;

  using LoadState = PrefetchContainerLoadState;
  void SetLoadState(LoadState prefetch_status);
  LoadState GetLoadState() const;

  const PrefetchRequest& request() const { return *request_; }

  // Controls ownership of the |ProxyLookupClientImpl| used during the
  // eligibility check.
  void TakeProxyLookupClient(
      std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client);
  std::unique_ptr<ProxyLookupClientImpl> ReleaseProxyLookupClient();

  // Called when it is added to `PrefetchService::owned_prefetches_`.
  void OnAddedToPrefetchService();

  // Whether or not the prefetch was determined to be eligibile.
  void OnEligibilityCheckComplete(PreloadingEligibility eligibility);

  // Adds a the new URL to |redirect_chain_|.
  void AddRedirectHop(const net::RedirectInfo& redirect_info);

  // The length of the redirect chain for this prefetch.
  size_t GetRedirectChainSize() const { return redirect_chain_.size(); }

  // Whether this prefetch is a decoy. Decoy prefetches will not store the
  // response, and not serve any prefetched resources.
  void SetIsDecoy(bool is_decoy) { is_decoy_ = is_decoy; }
  bool IsDecoy() const { return is_decoy_; }

  // Whether the prefetch request is cross-site/cross-origin for given origin.
  bool IsCrossSiteRequest(const url::Origin& origin) const;
  bool IsCrossOriginRequest(const url::Origin& origin) const;

  // Whether this prefetch is potentially contaminated by cross-site state.
  // If so, it may need special handling for privacy.
  // See https://crbug.com/1439246.
  bool IsCrossSiteContaminated() const { return is_cross_site_contaminated_; }
  void MarkCrossSiteContaminated();

  // Allows for |PrefetchCookieListener|s to be reigsitered for
  // `GetCurrentSingleRedirectHopToPrefetch()`.
  void RegisterCookieListener(network::mojom::CookieManager* cookie_manager);
  void PauseAllCookieListeners();
  void ResumeAllCookieListeners();

  // The network context used to make network requests, copy cookies, etc. for
  // the given `is_isolated_network_context_required`.
  PrefetchNetworkContext* GetNetworkContext(
      bool is_isolated_network_context_required) const;

  // The network context used to make network requests for the next prefetch.
  PrefetchNetworkContext* GetOrCreateNetworkContextForCurrentPrefetch();

  // Closes idle connections for all elements in |network_contexts_|.
  void CloseIdleConnections();

  // Set the currently prefetching |PrefetchStreamingURLLoader|.
  void SetStreamingURLLoader(
      base::WeakPtr<PrefetchStreamingURLLoader> streaming_loader);

  // Returns the URL loader being used for prefetching the current redirect hop.
  // This method should be used during prefetching and shouldn't be called for
  // serving purpose.
  base::WeakPtr<PrefetchStreamingURLLoader> GetStreamingURLLoader() const;

  bool IsStreamingURLLoaderDeletionScheduledForTesting() const;

  // Returns the PrefetchResponseReader of the prefetched non-redirect response
  // if already received its head. Ruturns nullptr otherwise.
  const PrefetchResponseReader* GetNonRedirectResponseReader() const;
  // Returns the head of the prefetched non-redirect response if already
  // received. Ruturns nullptr otherwise.
  const network::mojom::URLResponseHead* GetNonRedirectHead() const;

  // Clears |streaming_loader_| and cancels its loading, if any of its
  // corresponding `PrefetchResponseReader` does NOT start serving. Currently
  // this itself doesn't mark `this` as failed and thus can leave `this`
  // stalled. Therefore, call this method only if `this` can be no longer used
  // for serving, e.g. on the destructor or when
  // `HaveDefaultContextCookiesChanged()` is true.
  // TODO(crbug.com/40064891): For callsites outside the destructor, remove the
  // call or mark `this` as failed, because the current behavior (== existing
  // behavior, previously as `ResetAllStreamingURLLoaders()`) might potentially
  // cause issues when there are multiple navigations using `this` concurrently.
  void CancelStreamingURLLoaderIfNotServing();

  // Returns whether or not this prefetch has been considered to serve for a
  // navigation in the past. If it has, then it shouldn't be used for any future
  // navigations.
  bool HasPrefetchBeenConsideredToServe() const;

  // See `OnPrefetchResponseCompletedCallback`.
  void OnPrefetchComplete(
      bool is_success,
      const network::URLLoaderCompletionStatus& completion_status);

  // Note: Even if this returns `kServable`, `CreateRequestHandler()` can still
  // fail (returning null handler) due to final checks. See also the comment for
  // `PrefetchResponseReader::CreateRequestHandler()`.
  PrefetchServableState GetServableState(
      base::TimeDelta cacheable_duration) const;
  PrefetchMatchResolverAction GetMatchResolverAction(
      base::TimeDelta cacheable_duration) const;

  // Starts blocking `PrefetchMatchResolver` until non-redirect response header
  // is determined or timeouted. `on_maybe_determined_head_callback` will be
  // called when
  //
  // - `PrefetchStreamingURLLoader` succeeded/failed to fetch non-redirect
  //   response header.
  // - The argument `timeout` is positive and timeouted.
  // - `PrefetchContainer` dtor if `kPrefetchUnblockOnCancel` enabled.
  void StartBlockUntilHead(base::OnceCallback<void(PrefetchContainer&)>
                               on_maybe_determined_head_callback,
                           base::TimeDelta timeout);
  // Called when non-redirect response header is determined, i.e.
  // `GetNonRedirectHead()` becomes immutable.
  //
  // This method must be called at most once in the lifecycle of
  // `PrefetchContainer`.
  void OnDeterminedHead(bool is_successful_determined_head);
  // Unblocks waiting `PrefetchMatchResolver`.
  //
  // This method can be called multiple times.
  void UnblockPrefetchMatchResolver();

  void StartTimeoutTimerIfNeeded(base::OnceClosure on_timeout_callback);

  // Returns the time between the prefetch request was sent and the time the
  // response headers were received. Not set if the prefetch request hasn't been
  // sent or the response headers haven't arrived.
  std::optional<base::TimeDelta> GetPrefetchHeaderLatency() const {
    return header_latency_;
  }

  // Allow for the serving page to metrics when changes to the prefetch occur.
  void SetServingPageMetrics(base::WeakPtr<PrefetchServingPageMetricsContainer>
                                 serving_page_metrics_container);
  void UpdateServingPageMetrics();

  // Returns request id to be used by DevTools and test utilities.
  const std::string& RequestId() const { return request_id_; }

  // Simulates state transitions for:
  // - Passing eligibility check successfully (`LoadState::kEligible`),
  // - About to start prefetching (`LoadState::kStarted`), and
  // - Completion of prefetching.
  // For correct transitions, the methods should be called in the following
  // order (note that the `Simulate*()` methods here doesn't simulate the
  // loader):
  // - `SimulatePrefetchEligibleForTest()`
  // - `SimulatePrefetchStartedForTest()`
  // - `SetStreamingURLLoader()`
  // - `SimulatePrefetchCompletedForTest()`
  void SimulatePrefetchEligibleForTest();
  void SimulatePrefetchStartedForTest();
  void SimulatePrefetchCompletedForTest();

  // Simulates a prefetch container that failed at the eligibility check
  // (`LoadState::FailedIneligible`).
  void SimulatePrefetchFailedIneligibleForTest(
      PreloadingEligibility eligibility);

  // Set a callback for waiting for prefetch completion in tests.
  using PrefetchResponseCompletedCallbackForTesting =
      base::RepeatingCallback<void(base::WeakPtr<PrefetchContainer>)>;
  static void SetPrefetchResponseCompletedCallbackForTesting(
      PrefetchResponseCompletedCallbackForTesting callback);

  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchData() const {
    return no_vary_search_data_;
  }
  // Sets `no_vary_search_data_` from `GetHead()`. Exposed for tests.
  void MaybeSetNoVarySearchData();

  // Called upon detecting a change to cookies within the redirect chain.
  //
  // Note that there are two paths:
  //
  // - Roughly speaking, when non-redirect header received and
  //   `PrefetchService`/`PrefetchContainer` detected cookies change of the head
  //   of redirect chain. `PrefetchMatchResolver` propagates it to other waiting
  //   prefetches as they share domain.
  // - When `PrefetchURLLoaderInterceptor::MaybeCreateLoader()` handles
  //   redirects in the serving prefetch.
  void OnDetectedCookiesChange(
      std::optional<bool>
          is_unblock_for_cookies_changed_triggered_by_this_prefetch_container);

  // Called when the prefetch request is started (i.e. the URL loader is created
  // & started).
  void OnPrefetchStarted();

  PrefetchServingHandle CreateServingHandle();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool IsExactMatch(const GURL& url) const;
  bool IsNoVarySearchHeaderMatch(const GURL& url) const;
  // Checks that the URL matches to the NoVarySearch hint with a precondition.
  //
  // The precondition is that a non redirect header is not received, as
  // NoVarySearch hint is a mechanism to wait prefetches that is expected to
  // receive NoVarySearch header.
  bool ShouldWaitForNoVarySearchHeader(const GURL& url) const;

  // Records metrics when serving result is determined.
  //
  // This is eventually called once for every `PrefetchContainer` put in
  // `PrefetchMatchResolver::candidates_`, i.e. those potentially matching
  // and expected to become servable at the head of
  // `PrefetchMatchResolver::FindPrefetch()`.
  //
  // This can be called multiple times, because this can be called for multiple
  // `PrefetchMatchResolver`s.
  void OnUnregisterCandidate(
      const GURL& navigated_url,
      bool is_served,
      PrefetchPotentialCandidateServingResult serving_result,
      bool is_nav_prerender,
      std::optional<base::TimeDelta> blocked_duration);

  // TODO(crbug.com/372186548): Revisit the semantics of
  // `IsLikelyAheadOfPrerender()`.
  //
  // Returns true iff this prefetch was triggered for ahead of prerender or was
  // migrated with such ones.
  //
  // Currently, we (`PrerendererImpl`) start a prefetch ahead of prerender just
  // before starting a prerender and make them race 1. to reduce fetch request
  // even if prerender failed and fell back to normal navigation, 2. to buy time
  // for renderer process initialization of prerender.
  //
  // This flag is to indicate it's likely there is a such concurrent-ish
  // prerender request that wants to claim this prefetch even if it is not
  // started to avoid duplicated network requests, and thus if this is true, we
  // go through `kBlockUntilHeadUntilEligibilityGot` code path.
  //
  // - This flag is set if `max_preloading_type` is `PreloadingType::kPrerender`
  //   on `PrefetchContainer::ctor`.
  // - This flag is updated with prefetch migration `MigrateNewlyAdded()`: If we
  //   replace existing `PrefetchContainer` with such prerender-initiated
  //   `PrefetchContainer` with the same `PrefetchKey`, then we also
  //   transitively set the flag for the existing `PrefetchContainer` as well,
  //   because we'll still anticipate the prerendering request to hit the
  //   existing `PrefetchContainer` as it has the same key.
  bool IsLikelyAheadOfPrerender() const {
    return is_likely_ahead_of_prerender_;
  }

  // Merge a new `prefetch_request` into this `PrefetchContainer`.
  //
  // See also `PrefetchService::AddPrefetchContainerInternal()`.
  void MergeNewPrefetchRequest(
      std::unique_ptr<const PrefetchRequest> prefetch_request);

  // Handles loader related events. Currently used for DevTools and metrics.
  void NotifyPrefetchRequestWillBeSent(
      const network::mojom::URLResponseHeadPtr* redirect_head);
  void NotifyPrefetchResponseReceived(
      const network::mojom::URLResponseHead& head);
  void NotifyPrefetchRequestComplete(
      const network::URLLoaderCompletionStatus& completion_status);
  std::optional<mojo::PendingRemote<network::mojom::DevToolsObserver>>
  MakeSelfOwnedNetworkServiceDevToolsObserver();

  bool is_in_dtor() const { return is_in_dtor_; }

  void OnServiceWorkerStateDetermined(
      PrefetchServiceWorkerState service_worker_state);
  PrefetchServiceWorkerState service_worker_state() const {
    return service_worker_state_;
  }

  // Methods only exposed for `PrefetchServingHandle`.
  const std::vector<std::unique_ptr<PrefetchSingleRedirectHop>>& redirect_chain(
      base::PassKey<PrefetchServingHandle>) const;
  void SetProbeResult(base::PassKey<PrefetchServingHandle>,
                      PrefetchProbeResult probe_result);
  static std::optional<PreloadingTriggeringOutcome>
  TriggeringOutcomeFromStatusForServingHandle(
      base::PassKey<PrefetchServingHandle>,
      PrefetchStatus prefetch_status);

  const PrefetchContainerMetrics& GetPrefetchContainerMetrics() const {
    return prefetch_container_metrics_;
  }

  bool HasPreloadPipelineInfoForMetrics(const PreloadPipelineInfo& other) const;

 protected:
  // Updates metrics based on the result of the prefetch request.
  void UpdatePrefetchRequestMetrics(
      const network::mojom::URLResponseHead* head);

 private:
  // Update |prefetch_status_| and report prefetch status to
  // DevTools without updating TriggeringOutcome.
  void SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
      PrefetchStatus prefetch_status);

  // Updates `request().attempt()`'s outcome and failure reason based on
  // `new_prefetch_status`.
  // This should only be called after the prefetch is started, because
  // `request().attempt()` is degined to record the outcome or failure of
  // started triggers.
  void SetTriggeringOutcomeAndFailureReasonFromStatus(
      PrefetchStatus new_prefetch_status);

  // Returns if WebContents-level UA overrides should be applied for a prefetch
  // request for `request_url`. Note that not only the User-Agent header but
  // also Client-Hints headers are affected by the UA overrides.
  // The returned value is for an initial guess and shouldn't be used without a
  // plan for the header validation (crbug.com/444065296).
  bool ShouldApplyUserAgentOverride(const GURL& request_url) const;
  // Adds the User-Agent header by UA override if applicable.
  void MaybeApplyOverrideForUserAgentHeader(
      network::ResourceRequest& resource_request);
  // Adds client hints headers to a request bound for |origin|.
  void AddClientHintsHeaders(const url::Origin& origin,
                             net::HttpRequestHeaders* request_headers);
  // Adds X-Client-Data request header to a request.
  void AddXClientDataHeader(network::ResourceRequest& request);

  // Returns the `PrefetchSingleRedirectHop` to be prefetched next.
  // This is the last element in `redirect_chain_`, because, during prefetching
  // from the network, we push back `PrefetchSingleRedirectHop`s to
  // `redirect_chain_` and access the latest redirect hop.
  PrefetchSingleRedirectHop& GetCurrentSingleRedirectHopToPrefetch() const;

  // Returns the `PrefetchSingleRedirectHop` for the redirect leg
  // before `GetCurrentSingleRedirectHopToPrefetch()`. This must be called only
  // if `this` has redirect(s).
  const PrefetchSingleRedirectHop& GetPreviousSingleRedirectHopToPrefetch()
      const;

  // Returns "Sec-Purpose" header value for a prefetch request to `request_url`.
  const char* GetSecPurposeHeaderValue(const GURL& request_url) const;

  // Called when a prefetch request could not be started because of eligibility
  // reasons. Should only be called for the initial prefetch request and not
  // redirects.
  void OnInitialPrefetchFailedIneligible(PreloadingEligibility eligibility);

  std::string GetMetricsSuffix() const;

  // Record `prefetch_status` to UMA if it hasn't already been recorded for this
  // container.
  // Note: We use a parameter instead of just `prefetch_status_` as it may not
  // be updated to the latest value when this method is called.
  void MaybeRecordPrefetchStatusToUMA(PrefetchStatus prefetch_status);

  // Records UMAs tracking some certain durations during prefetch addition to
  // prefetch completion (e.g. `Prefetch.PrefetchContainer.AddedTo*`).
  void RecordPrefetchDurationHistogram();
  // Records `Prefetch.PrefetchContainer.PrefetchMatchMissedToPrefetchStarted.*`
  // UMA.
  void RecordPrefetchMatchMissedToPrefetchStartedHistogram();
  // Records `Prefetch.PrefetchMatchingBlockedNavigationWithPrefetch.*` UMAs.
  void RecordPrefetchMatchingBlockedNavigationHistogram(bool blocked_until_head,
                                                        bool is_nav_prerender);
  // Records `Prefetch.PrefetchContainer.ServedCount`.
  void RecordPrefetchContainerServedCountHistogram();

  // Records `Prefetch.BlockUntilHeadDuration.*` UMAs.
  void RecordBlockUntilHeadDurationHistogram(
      const std::optional<base::TimeDelta>& blocked_duration,
      bool served,
      bool is_nav_prerender);
  // Records
  // `Prefetch.PrefetchPotentialCandidateServingResult.PerMatchingCandidate.*`
  // UMAs.
  void RecordPrefetchPotentialCandidateServingResultHistogram(
      PrefetchPotentialCandidateServingResult matching_result);

  // Should be called only from `OnPrefetchComplete()`, so that
  // `OnPrefetchCompletedOrFailed()` is always called after
  // `OnPrefetchCompleteInternal()`.
  void OnPrefetchCompleteInternal(
      const network::URLLoaderCompletionStatus& completion_status);

  PrefetchServableState GetServableStateInternal(
      base::TimeDelta cacheable_duration) const;

  // The prefetch request parameters of the very first initiator/requester of
  // this prefetch at the time of request creation.
  // This should be immutable. If we need to have modified parameters updated
  // over time or reflect the parameters of non-first requesters, then the
  // modified parameters/non-first-requester parameters should be
  // `PrefetchContainer` members outside `request_`.
  const std::unique_ptr<const PrefetchRequest> request_;

  PrefetchServiceWorkerState service_worker_state_ =
      PrefetchServiceWorkerState::kAllowed;

  // The referrer to use for the request. This is updated through redirects.
  blink::mojom::Referrer referrer_;

  // Information about the current prefetch request. Updated when a redirect is
  // encountered, whether or not the direct can be processed by the same URL
  // loader or requires the instantiation of a new loader.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // The No-Vary-Search response data, parsed from the actual response header
  // (`GetHead()`).
  // Unless this is set, `no_vary_search` helpers don't perform No-Vary-Search
  // matching for `this`, even if `GetHead()` has No-Vary-Search headers.
  std::optional<net::HttpNoVarySearchData> no_vary_search_data_;

  // The current status, if any, of the prefetch.
  // TODO(crbug.com/40075414): Use `load_state_` instead for non-metrics
  // purpose.
  std::optional<PrefetchStatus> prefetch_status_;
  bool prefetch_status_recorded_to_uma_ = false;

  // True iff `PrefetchStatus` was set to `kPrefetchNotUsedCookiesChanged` once.
  //
  // TODO(crbug.com/40075414): Remove this.
  bool on_detected_cookies_change_called_ = false;

  // The current status of the prefetch.
  LoadState load_state_ = LoadState::kNotStarted;

  // Looks up the proxy settings in the default network context all URLs in
  // |redirect_chain_|.
  std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client_;

  // Whether this prefetch is a decoy or not. If the prefetch is a decoy then
  // any prefetched resources will not be served.
  bool is_decoy_ = false;

  // The redirect chain resulting from prefetching |GetURL()|.
  std::vector<std::unique_ptr<PrefetchSingleRedirectHop>> redirect_chain_;

  // The network contexts used for this prefetch. They key corresponds to the
  // |is_isolated_network_context_required| param of the
  // |PrefetchNetworkContext|.
  std::map<bool, std::unique_ptr<PrefetchNetworkContext>> network_contexts_;

  // The currently prefetching streaming URL loader, prefetching the last
  // element of `redirect_chain_`. Multiple streaming URL loaders can be used in
  // the event a redirect causes a change in the network context, but here only
  // one (=last) `PrefetchStreamingURLLoader` is kept here, because when
  // switching the network context and `PrefetchStreamingURLLoader`s, the old
  // `PrefetchStreamingURLLoader` is scheduled for deletion and then the new
  // `PrefetchStreamingURLLoader` is set here.
  base::WeakPtr<PrefetchStreamingURLLoader> streaming_loader_;

  // The amount of time it took for the headers to be received.
  std::optional<base::TimeDelta> header_latency_;

  // Counts how many times this container has been served to the navigation.
  // Only used for the metrics.
  base::ClampedNumeric<uint32_t> served_count_ = 0;

  // The result of probe when checked on navigation.
  std::optional<PrefetchProbeResult> probe_result_;

  // If set, this prefetch's timing might be affected by cross-site state, so
  // further processing may need to affect how the response is processed to make
  // inferences about this logic less practical.
  bool is_cross_site_contaminated_ = false;

  // Reference to metrics related to the page that considered using this
  // prefetch.
  base::WeakPtr<PrefetchServingPageMetricsContainer>
      serving_page_metrics_container_;

  // Request identifier used by DevTools and test utilities.
  std::string request_id_;

  // Information of preload pipeline that this prefetch belongs/is related to.
  //
  // If a prerender triggers a prefetch ahead of prerender, it needs to get to
  // know information of the prefetch, e.g eligibility, to judge to abort
  // prerender when prefetch failed. Unfortunately we can't pass the information
  // at the prefetch matching process, as prefetch may fail before it and other
  // `NavigationLoaderInterceptor` e.g. one of service worker can intercept.
  //
  // So, we pass such information via pipeline infos.
  //
  // - `redirect_chain_[0].eligibility_`
  // - `prefetch_status_`
  //
  // The values must be synchronized both when these fields are updated and when
  // a new pipeline info added to `inherited_preload_pipeline_infos_`.
  //
  // A new pipeline info added when another prefetch is migrated into it. See
  // `MigrateNewlyAdded()`.
  //
  // Note that we distinguish the primary one
  // (`request().preload_pipeline_info()`) and inherited ones
  // (`inherited_preload_pipeline_infos_`) because we send CDP events with id of
  // the primary one.
  std::vector<scoped_refptr<PreloadPipelineInfoImpl>>
      inherited_preload_pipeline_infos_;

  // The time at which |PrefetchService| started blocking until the head of
  // |this| was received.
  std::optional<base::TimeTicks> blocked_until_head_start_time_;

  // A timer used to limit the maximum amount of time that a navigation can be
  // blocked waiting for the head of this prefetch to be received.
  std::unique_ptr<base::OneShotTimer> block_until_head_timer_;

  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  // True iff the destructor was called.
  bool is_in_dtor_ = false;

  base::ObserverList<Observer> observers_;

  bool is_likely_ahead_of_prerender_ = false;

  // The time that the latest earlier prefetch unmatch happened that this
  // prefetch could've been served to.
  // Set via `SetPrefetchMatchMissedTimeForMetrics()` which can be called during
  // prefetch start (`PrefetchService::StartSinglePrefetch()`).
  std::optional<base::TimeTicks> time_prefetch_match_missed_;

  PrefetchContainerMetrics prefetch_container_metrics_;

  base::WeakPtrFactory<PrefetchContainer> weak_method_factory_{this};
};

// For debug logs.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& ostream,
    const PrefetchContainer& prefetch_container);

CONTENT_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                        PrefetchContainer::LoadState state);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
