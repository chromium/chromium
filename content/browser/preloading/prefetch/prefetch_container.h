// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_

#include <optional>

#include "base/auto_reset.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/preloading/prefetch/pre_prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/browser/preloading/preload_pipeline_info_impl.h"
#include "content/browser/preloading/preload_serving_metrics.h"
#include "content/common/content_export.h"
#include "content/public/browser/prefetch_deduplication_utils.h"
#include "content/public/browser/preloading.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_no_vary_search_data.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/devtools_observer.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"
#include "url/gurl.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace network {
class SharedURLLoaderFactory;
namespace mojom {
class NetworkContext;
}  // namespace mojom
}  // namespace network

namespace content {

class AssertPrefetchContainerObserver;
class PrefetchContainerObserver;
class PrefetchIsolatedNetworkContext;
class PrefetchKey;
class PrefetchMatchResolverAction;
class PrefetchRequest;
class PrefetchResponseReader;
class PrefetchService;
class PrefetchServingHandle;
class PrefetchServingPageMetricsContainer;
class PrefetchSingleRedirectHop;
class PrefetchStreamingURLLoader;
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

// The primary prefetching state of `PrefetchContainer`.
//
// The valid transitions and correspondence to
// `PrefetchResponseReader::LoadState` are also described by the design doc
// https://docs.google.com/document/d/1OgX1e6dbqYhXUE4_AUm3TE4g_0bC2u48QmHiJiUhGxU/edit?usp=sharing
// and are verified by:
// - `base::StateTransitions` in `PrefetchContainer::SetLoadState()` and
// - `AssertPrefetchContainerObserver`.
//
// Note: there are related states like `request().attempt()`'s triggering
// outcome and failure info, `PrefetchStatus` etc., but prefer using
// `PrefetchContainerLoadState` as long as possible.
// These other states intentionally don't directly affect
// `PrefetchContainerLoadState` and `PrefetchResponseReader`'s servability.
// (e.g. it can be servable even if `request().attempt()` has a failure)
//
// TODO(https://crbug.com/432518638): Make `PrefetchContainerLoadState` and
// `PrefetchResponseReader::LoadState` more strictly/directly correspond and
// verify it by adding CHECK()s.
//
// TODO(https://crbug.com/400761083): Always reach to `kCompleted` or `kFailed`.
// The only remaining case is
// `PrefetchResponseReader::LoadState::kFailedRedirect`.
enum class PrefetchContainerLoadState {
  // --- Phase 1. [Initial state]
  kNotStarted,

  // --- Phase 2. The eligibility check for the initial request has completed.
  // Non-redirect `PrefetchContainer::OnEligibilityCheckComplete()` and
  // `PreloadingAttempt::SetEligibility()` have been called.
  // [Observer] `PrefetchContainerObserver::OnGotInitialEligibility()`.

  kEligible,
  // [Final state]
  kFailedIneligible,

  // --- Phase 3. The holdback check has completed.
  // `PrefetchService::StartSinglePrefetch()` has been called.

  // [Final state] Heldback due to `PreloadingAttempt::ShouldHoldback()`.
  kFailedHeldback,

  // Prefetch is started. On or after this state:
  // - `PrefetchContainer` has corresponding `PrefetchResponseReader`(s), and
  //   `PrefetchContainerLoadState` and `PrefetchResponseReader::LoadState` are
  //   aligned.
  // - `PreloadingAttempt::SetFailureReason()` can be called.
  kStarted,

  // --- Phase 4. The non-redirect prefetch response is determined.
  // `PrefetchContainer::OnDeterminedHead()` has been called.
  // [Observer] `PrefetchContainerObserver::OnDeterminedHead()`.
  kDeterminedHead,
  kFailedDeterminedHead,

  // --- Phase 5. [Final state] The prefetch completed successfully or failed.
  // `PrefetchContainer::OnPrefetchComplete()` has been called.
  // [Observer] `PrefetchContainerObserver::OnPrefetchCompletedOrFailed()`.
  kCompleted,
  kFailed,
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
//
// Do not pass `PrefetchContainer` to outside //content using
// `PrefetchDeduplicationEntry`.
class CONTENT_EXPORT PrefetchContainer
    : public content::PrefetchDeduplicationEntry {
 public:
  // In non-test, `PrefetchContainer` should be only created and owned by
  // `PrefetchService`.
  static std::unique_ptr<PrefetchContainer> Create(
      base::PassKey<PrefetchService>,
      std::unique_ptr<const PrefetchRequest> request,
      std::unique_ptr<PrePrefetchContainer> pre_prefetch_container);
  static std::unique_ptr<PrefetchContainer> CreateForTesting(
      std::unique_ptr<const PrefetchRequest> request,
      std::unique_ptr<PrePrefetchContainer> pre_prefetch_container = nullptr);

  // Called when the response for |prefetch_container| has started. Based on
  // |head|, returns a status to inform the |PrefetchStreamingURLLoader| whether
  // the prefetch is servable. If servable, then `std::nullopt` will be
  // returned, otherwise a failure status is returned.
  // This is `static` just to handle `WeakPtr` check.
  static std::optional<PrefetchErrorOnResponseReceived>
  OnPrefetchResponseStarted(base::WeakPtr<PrefetchContainer> prefetch_container,
                            network::mojom::URLResponseHead* head);

  // Use `Create*()` above.
  // TODO(crbug.com/452389538): Receiving `PrefetchRequest` and
  // `PrePrefetchContainer` whose only `PrefetchRequest` is moved out (when
  // `PrefetchService::AddPrefetchRequestFromPrePrefetch()`) is confusing.
  // Revisit to find a better interface.
  PrefetchContainer(
      base::PassKey<PrefetchContainer>,
      std::unique_ptr<const PrefetchRequest> request,
      std::unique_ptr<PrePrefetchContainer> pre_prefetch_container);

  ~PrefetchContainer() override;

  PrefetchContainer(const PrefetchContainer&) = delete;
  PrefetchContainer& operator=(const PrefetchContainer&) = delete;

  // ----------------------------------------------------------------
  // Values that never change throughout `PrefetchContainer` lifetime.

  const PrefetchRequest& request() const { return *request_; }

  // Equivalent to `request().key()`.
  const PrefetchKey& key() const;

  // `content::PrefetchDeduplicationEntry` overrides.
  // The initial URL that was requested to be prefetched.
  // Equivalent to `request().key().url()`.
  const GURL& GetURL() const override;

  // Equivalent to `request().no_vary_search_hint()`.
  // Exposed for `PrefetchMatchResolver`.
  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint()
      const override;

  // ----------------------------------------------------------------
  // Values that can become non-null upon transitioning to
  // `PrefetchContainerLoadState::kEligible` or
  // `PrefetchContainerLoadState::kFailedIneligible`, and never change after
  // that.

  // Returns non-null if and only if after transitioning to
  // `PrefetchContainerLoadState::kEligible` or
  // `PrefetchContainerLoadState::kFailedIneligible`.
  const std::optional<PreloadingEligibility>& GetInitialEligibility() const {
    return initial_eligibility_;
  }

  // ----------------------------------------------------------------
  // Values that can become non-null upon transitioning to
  // `PrefetchContainerLoadState::kDeterminedHead` or
  // `PrefetchContainerLoadState::kFailedDeterminedHead`, and never change after
  // that.

  // `GetNonRedirect*()` methods return the `PrefetchResponseReader` or
  // `ResponseHead` of the prefetched non-redirect response, respectively, if
  // already received its head. Ruturns nullptr otherwise.
  // Note: These can return non-null even on
  // `PrefetchContainerLoadState::kFailed`.
  //
  // More precisely, returns non-null on:
  // - `PrefetchContainerLoadState::kDeterminedHead` (always)
  // - `PrefetchContainerLoadState::kCompleted` (always)
  // - `PrefetchContainerLoadState::kFailedDeterminedHead` (in some cases)
  // - `PrefetchContainerLoadState::kFailed` (in some cases)
  // (See also the comment of `PrefetchResponseReader::GetHead()`)
  //
  // Note: When `GetNonRedirect*()` methods return non-null, it always points to
  // the final `PrefetchResponseReader` and isn't affected by
  // https://crbug.com/432518638, because when the non-redirect response is
  // received all redirects are already completed.
  const PrefetchResponseReader* GetNonRedirectResponseReader() const;
  const network::mojom::URLResponseHead* GetNonRedirectHead() const;

  // Returns the HTTP response code of the non-redirect response, if received.
  // Note that this returns the response code even on failures, e.g. when a
  // non-2xx response is received, or the prefetch is failed after response is
  // received. This is for the intentional usage of getting the non-2xxx
  // response code for failed response.
  std::optional<int> GetResponseCode() const;

  // ----------------------------------------------------------------
  // Values that can become non-null upon transitioning to
  // `PrefetchContainerLoadState::kCompleted` or
  // `PrefetchContainerLoadState::kFailed`, and
  // never change after that.

  // Returns the completion status. This returns non-null if and only if on
  // `PrefetchContainerLoadState::kCompleted` or
  // `PrefetchContainerLoadState::kFailed`.
  const std::optional<network::URLLoaderCompletionStatus>& GetCompletionStatus()
      const;

  // ----------------------------------------------------------------

  // Returns `true` if the `prefetch_container` is stale. I.e.
  // the prefetch either is not or never will be servable to a
  // navigation.
  //
  // Note: This is currently used for WebView initiated prefetches so
  // consideration should be taken if updating the underlying implementation (or
  // its dependencies).
  bool IsPrefetchStale() const override;

  // The current URL being fetched.
  GURL GetCurrentURL() const;

  // Whether or not an isolated network context is required to the next
  // prefetch.
  bool IsIsolatedNetworkContextRequiredForCurrentPrefetch() const;

  // Whether or not an isolated network context is required for the previous
  // redirect hop of the given url.
  bool IsIsolatedNetworkContextRequiredForPreviousRedirectHop() const;

  base::WeakPtr<PrefetchResponseReader> GetResponseReaderForCurrentPrefetch();

  const network::ResourceRequest* GetResourceRequest() const {
    return resource_request_.get();
  }

  const GURL& GetActivationBeaconUrl() const { return activation_beacon_url_; }

  base::WeakPtr<PrefetchContainer> GetWeakPtr() {
    return weak_method_factory_.GetWeakPtr();
  }
  base::WeakPtr<const PrefetchContainer> GetWeakPtr() const {
    return weak_method_factory_.GetWeakPtr();
  }
  // Be cautious when using this, as this is like a const cast.
  base::WeakPtr<PrefetchContainer> GetMutableWeakPtr() const {
    return weak_method_factory_.GetMutableWeakPtr();
  }

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

  // Whether or not the prefetch was determined to be eligibile.
  void OnEligibilityCheckComplete(PreloadingEligibility eligibility);

  // Adds `url` (the next URL to prefetch) to |redirect_chain_|.
  void AddRedirectHop(const GURL& url);

  // Performs the actual modification to `resource_request_` upon redirect.
  void UpdateResourceRequest(
      const net::RedirectInfo& redirect_info,
      const network::HttpRequestHeadersUpdateParams& headers_update_params);

  // Whether this prefetch is a decoy. Decoy prefetches will not store the
  // response, and not serve any prefetched resources.
  void SetIsDecoy(bool is_decoy) { is_decoy_ = is_decoy; }
  bool IsDecoy() const { return is_decoy_; }

  // Whether this prefetch is potentially contaminated by cross-site state.
  // If so, it may need special handling for privacy.
  // See https://crbug.com/1439246.
  bool IsCrossSiteContaminated() const { return is_cross_site_contaminated_; }
  void MarkCrossSiteContaminated();

  // Allows for |PrefetchCookieListener|s to be registered for
  // `GetCurrentSingleRedirectHopToPrefetch()`.
  void PauseAllCookieListeners();
  void ResumeAllCookieListeners();

  // The isolated network context used to make network requests, copy cookies,
  // etc. This is a per-`PrefetchContainer` instance that can be used for all
  // redirect hops where needed.
  // This returns `nullptr` when the isolated network context for `this` is not
  // yet created.
  PrefetchIsolatedNetworkContext* GetIsolatedNetworkContext() const;

  // Creates the isolated network context.
  PrefetchIsolatedNetworkContext* CreateIsolatedNetworkContext(
      mojo::Remote<network::mojom::NetworkContext> isolated_network_context);

  scoped_refptr<network::SharedURLLoaderFactory>
  GetOrCreateDefaultNetworkContextURLLoaderFactory();

  // Closes idle connections for all isolated network contexts.
  void CloseIdleConnections();

  // Set the currently prefetching |PrefetchStreamingURLLoader|.
  void SetStreamingURLLoader(
      base::WeakPtr<PrefetchStreamingURLLoader> streaming_loader);

  // Returns the URL loader being used for prefetching the current redirect hop.
  // This method should be used during prefetching and shouldn't be called for
  // serving purpose.
  base::WeakPtr<PrefetchStreamingURLLoader> GetStreamingURLLoader() const;

  bool IsStreamingURLLoaderDeletionScheduledForTesting() const;

  // Clears |streaming_loader_| and cancels its loading, if any of its
  // corresponding `PrefetchResponseReader` does NOT start serving.
  // This sets/notifies of a failure when called outside `PrefetchContainer`
  // dtor, so always call this asynchronously outside the dtor.
  void CancelStreamingURLLoaderIfNotServing();

  // See `OnPrefetchResponseCompletedCallback`.
  void OnPrefetchComplete(
      bool is_success,
      const network::URLLoaderCompletionStatus& completion_status);

  // Note: Even if `GetMatchResolverAction().ToServableState()` is `kServable`,
  // `CreateRequestHandler()` can still fail (returning null handler) due to
  // final checks. See also the comment for
  // `PrefetchResponseReader::CreateRequestHandler()`.
  PrefetchMatchResolverAction GetMatchResolverAction() const;

  // Called when non-redirect response header is determined, i.e.
  // `GetNonRedirectHead()` becomes immutable.
  //
  // This method must be called at most once in the lifecycle of
  // `PrefetchContainer`.
  void OnDeterminedHead(bool is_successful_determined_head);

  void StartTimeoutTimerIfNeeded(base::OnceClosure on_timeout_callback);

  // ----------------------------------------------------------------
  // Testing:

  // Returns the container id used by test utilities.
  const std::string& ContainerIdForTesting() const {
    return container_id_for_testing_;
  }

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
  void SimulatePrefetchEligibleForTest();
  void SimulatePrefetchStartedForTest();
  void SimulatePrefetchRedirectedForTest(
      const GURL& redirect_url,
      PreloadingEligibility eligibility = PreloadingEligibility::kEligible);

  // Simulates a prefetch container that failed at the eligibility check
  // (`LoadState::FailedIneligible`).
  void SimulatePrefetchFailedIneligibleForTest(
      PreloadingEligibility eligibility);

  // Set a callback for waiting for prefetch completion in tests.
  using PrefetchResponseCompletedCallbackForTesting =
      base::RepeatingCallback<void(base::WeakPtr<PrefetchContainer>)>;
  static void SetPrefetchResponseCompletedCallbackForTesting(
      PrefetchResponseCompletedCallbackForTesting callback);

  const std::optional<net::HttpNoVarySearchData>&
  GetNoVarySearchDataForTesting() const {
    return no_vary_search_data_;
  }

  // ----------------------------------------------------------------

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

  // Only for temporary const queries to `PrefetchServingHandle`, namely
  // `HaveDefaultContextCookiesChanged()`.
  std::unique_ptr<const PrefetchServingHandle> CreateConstServingHandle() const;

  void AddObserver(PrefetchContainerObserver* observer);
  void RemoveObserver(PrefetchContainerObserver* observer);

  bool IsExactMatch(const GURL& url) const;
  bool IsNoVarySearchHeaderMatch(const GURL& url) const;

  // Returns true if this `PrefetchContainer` was constructed from a
  // `PrePrefetchContainer`.
  bool IsConstructedFromPrePrefetch() const;

  // Returns true if both `pre_prefetch_loader_` and
  // `pre_prefetch_loader_client_receiver_` are valid, which means the
  // initial request can be made by `PrePrefetchContainer`.
  bool ExistsValidPrePrefetch() const;

  // Uses `pre_prefetch_loader_` and `pre_prefetch_loader_client_receiver_` to
  // create a new `URLLoaderFactory` that returns the pre-prefetch when
  // `CreateLoaderAndStart()` is called on the first redirect hop.
  scoped_refptr<network::SharedURLLoaderFactory>
  CreatePrePrefetchURLLoaderFactory();

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

  // ----------------------------------------------------------------
  // Metrics:

  // Sets the time that the latest earlier prefetch unmatch happened that this
  // prefetch could've been served to. Please see
  // `time_prefetch_match_missed_` for more details.
  void SetPrefetchMatchMissedTimeForMetrics(base::TimeTicks time);

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

  const PrefetchContainerMetrics& GetPrefetchContainerMetrics() const {
    return prefetch_container_metrics_;
  }

  bool HasPreloadPipelineInfoForMetrics(const PreloadPipelineInfo& other) const;

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

  template <typename Method, typename... Args>
  void NotifyObservers(Method method, const Args&... args) {
    base::AutoReset<bool> auto_reset(&during_observer_notification_, true);
    observers_.Notify(method, *this, args...);
  }

  // ----------------------------------------------------------------
  // Prefetching:

  // The previous URL, if this has been redirected. Invalid to call otherwise.
  GURL GetPreviousURL() const;

  // Returns the devtools request id that should be set to resource request
  // during `OnPrefetchStarted()`.
  // Note that this is also called via
  // `SetPrefetchStatusWithoutUpdatingTriggeringOutcome()`, where resource
  // request might not yet created.
  const std::string& GetDevtoolsRequestId() const;

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

  std::optional<PrefetchErrorOnResponseReceived>
  OnPrefetchResponseStartedInternal(network::mojom::URLResponseHead* head);

  // Should be called only from `OnPrefetchComplete()`, so that
  // `OnPrefetchCompletedOrFailed()` is always called after
  // `OnPrefetchCompleteInternal()`.
  void OnPrefetchCompleteInternal();

  void NotifyPrefetchResponseReceived(
      const network::mojom::URLResponseHead& head);
  void NotifyPrefetchRequestComplete();

  // ----------------------------------------------------------------
  // Metrics:

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

  // Updates metrics based on the result of the prefetch request.
  void UpdatePrefetchRequestMetrics(
      const network::mojom::URLResponseHead* head);

  // The prefetch request parameters of the very first initiator/requester of
  // this prefetch at the time of request creation.
  // This should be immutable. If we need to have modified parameters updated
  // over time or reflect the parameters of non-first requesters, then the
  // modified parameters/non-first-requester parameters should be
  // `PrefetchContainer` members outside `request_`.
  const std::unique_ptr<const PrefetchRequest> request_;

  // True if this `PrefetchContainer` was constructed from a
  // `PrePrefetchContainer`.
  const bool is_constructed_from_pre_prefetch_ = false;

  PrefetchServiceWorkerState service_worker_state_ =
      PrefetchServiceWorkerState::kAllowed;

  // The `ResourceRequest` for the current prefetch request.
  // This is initially set in `OnPrefetchStarted()`, which is called from
  // `PrefetchService` when Prefetch is dequeued and its request is actually
  // started.
  // - For normal prefetches, this is created from `PrefetchRequest`.
  // - For PrePrefetch-initiated prefetches, this is passed from ctor.
  // This is updated upon redirects, regardless of whether the redirect is
  // handled by the same URL loader or requires a new loader.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // ResourceRequest that was used for `PrePrefetch`. This should eventually be
  // moved to `resource_request_` when the initial resource request after the
  // validation succeeds.
  std::unique_ptr<network::ResourceRequest> resource_request_for_pre_prefetch_;

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

  // Whether this prefetch is a decoy or not. If the prefetch is a decoy then
  // any prefetched resources will not be served.
  bool is_decoy_ = false;

  // The redirect chain resulting from prefetching |GetURL()|.
  std::vector<std::unique_ptr<PrefetchSingleRedirectHop>> redirect_chain_;

  // The eligibility for the initial request. This is not placed in
  // `redirect_chain_[0]` to avoid the dependencies from
  // https://crbug.com/432518638.
  std::optional<PreloadingEligibility> initial_eligibility_;

  // The completion status of prefetch. This is non-null if and only if on
  // `PrefetchContainerLoadState::kCompleted` or
  // `PrefetchContainerLoadState::kFailed`.
  // TODO(crbug.com/432518638): Refer to the last
  // `PrefetchResponseReader::completion_status_`. Currently failed completion
  // can happen on non-last `PrefetchResponseReader`.
  std::optional<network::URLLoaderCompletionStatus> completion_status_;

  // The network contexts used for this prefetch.
  scoped_refptr<network::SharedURLLoaderFactory>
      default_network_context_url_loader_factory_;
  std::unique_ptr<PrefetchIsolatedNetworkContext> isolated_network_context_;

  // The currently prefetching streaming URL loader, prefetching the last
  // element of `redirect_chain_`. Multiple streaming URL loaders can be used in
  // the event a redirect causes a change in the network context, but here only
  // one (=last) `PrefetchStreamingURLLoader` is kept here, because when
  // switching the network context and `PrefetchStreamingURLLoader`s, the old
  // `PrefetchStreamingURLLoader` is scheduled for deletion and then the new
  // `PrefetchStreamingURLLoader` is set here.
  base::WeakPtr<PrefetchStreamingURLLoader> streaming_loader_;

  // The URLLoader and URLLoaderClient created when PrePrefetch.
  // This should be moved from `PrePrefetchContainer` when `this` ctor, and
  // connected to PrefetchStreamingURLLoader when starting Prefetch.
  // It should be consumed after `CreatePrePrefetchURLLoaderFactory()`.
  mojo::PendingRemote<network::mojom::URLLoader> pre_prefetch_loader_;
  mojo::PendingReceiver<network::mojom::URLLoaderClient>
      pre_prefetch_loader_client_receiver_;

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

  // Container id used by test utilities.
  const std::string container_id_for_testing_;

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

  // True during notifying `observers_`.
  // This is used to `DUMP_WILL_BE_CHECK()` the disallowed operations during
  // `PrefetchContainerObserver` callbacks. Theoretically there can still be
  // violating corner cases, so `DUMP_WILL_BE_CHECK()` is used, to first monitor
  // if there are actual violations in the wild.
  bool during_observer_notification_ = false;

  base::ObserverList<PrefetchContainerObserver> observers_{
      base::ObserverListPolicy::EXISTING_ONLY};

  bool is_likely_ahead_of_prerender_ = false;

  // The time that the latest earlier prefetch unmatch happened that this
  // prefetch could've been served to.
  // Set via `SetPrefetchMatchMissedTimeForMetrics()` which can be called during
  // prefetch start (`PrefetchService::StartSinglePrefetch()`).
  std::optional<base::TimeTicks> time_prefetch_match_missed_;

  // The resolved and validated full URL for the activation beacon.
  GURL activation_beacon_url_;

  PrefetchContainerMetrics prefetch_container_metrics_;

  std::unique_ptr<AssertPrefetchContainerObserver> assert_observer_;

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
