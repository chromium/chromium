// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_

#include <optional>
#include <utility>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/speculation_host_devtools_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/prefetch_browser_callbacks.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "url/gurl.h"

namespace network {
namespace mojom {
class CookieManager;
}  // namespace mojom
}  // namespace network

namespace content {

class BrowserContext;
class PrefetchCookieListener;
class PrefetchDocumentManager;
class PrefetchNetworkContext;
class PrefetchResponseReader;
class PrefetchService;
class PrefetchServingPageMetricsContainer;
class PrefetchStreamingURLLoader;
class PreloadingAttempt;
class ProxyLookupClientImpl;
class RenderFrameHost;
class RenderFrameHostImpl;

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

// This class contains the state for a request to prefetch a specific URL.
//
// A `PrefetchContainer` can have multiple `PrefetchContainer::SinglePrefetch`es
// and `PrefetchStreamingURLLoader`s to support redirects. Each
// `PrefetchContainer::SinglePrefetch` in `redirect_chain_` corresponds to a
// single redirect hop, while a single `PrefetchStreamingURLLoader` can receive
// multiple redirect hops unless network context switching is needed.
//
// For example:
//
// |PrefetchStreamingURLLoader A-----| |PrefetchStreamingURLLoader B ---------|
// HandleRedirect  - HandleRedirect  - HandleRedirect  - ReceiveResponse-Finish
// |SinglePrefetch0| |SinglePrefetch1| |SinglePrefetch2| |SinglePrefetch3-----|
//
// While prefetching (see methods named like "ForCurrentPrefetch" or
// "ToPrefetch"), `SinglePrefetch`es and `PrefetchStreamingURLLoader`s (among
// other members) are added and filled. The steps for creating these objects and
// associating with each other span multiple classes/methods:
//
// 1. A new `PrefetchContainer::SinglePrefetch` and thus a new
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
  // Ctor used for renderer-initiated prefetch.
  PrefetchContainer(
      RenderFrameHostImpl& referring_render_frame_host,
      const blink::DocumentToken& referring_document_token,
      const GURL& url,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referrer,
      std::optional<net::HttpNoVarySearchData> no_vary_search_expected,
      base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager,
      base::WeakPtr<PreloadingAttempt> attempt = nullptr);

  // Ctor used for browser-initiated prefetch.
  // We can pass the referring origin of prefetches via `referring_origin` if
  // necessary. When `std::nullopt` is passed, the referring origin will be
  // opaque.
  PrefetchContainer(
      WebContents& referring_web_contents,
      const GURL& url,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referrer,
      const std::optional<url::Origin>& referring_origin,
      std::optional<net::HttpNoVarySearchData> no_vary_search_expected,
      base::WeakPtr<PreloadingAttempt> attempt = nullptr,
      std::optional<PreloadingHoldbackStatus> holdback_status_override =
          std::nullopt);

  // Ctor used for browser-initiated prefetch that doesn't depend on web
  // contents. We can pass the referring origin of prefetches via
  // `referrer_origin` if necessary. When `std::nullopt` is passed, the
  // referring origin will be opaque.
  PrefetchContainer(
      BrowserContext* browser_context,
      const GURL& url,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referrer,
      bool javascript_enabled,
      const std::optional<url::Origin>& referring_origin,
      std::optional<net::HttpNoVarySearchData> no_vary_search_expected,
      base::WeakPtr<PreloadingAttempt> attempt = nullptr,
      std::optional<PrefetchStartCallback> prefetch_start_callback =
          std::nullopt);

  ~PrefetchContainer();

  PrefetchContainer(const PrefetchContainer&) = delete;
  PrefetchContainer& operator=(const PrefetchContainer&) = delete;

  // Key for managing and matching prefetches.
  //
  // This key can either represent
  //
  // - the key of a prefetch (typically named `prefetch_key`, and its URL is the
  //   URL of the prefetched main resource); or
  // - the key of a navigation (typically named `navigated_key`, and its URL is
  //   the navigation request URL).
  //
  // TODO(crbug.com/364751887): This distinction is not perfect. Enforce it as
  // much as possible.
  //
  // For prefetch, non URL part is given as the following:
  //
  // - If the prefetch is renderer-initiated, `DocumentToken` of the initiating
  //   document is used.
  // - If the prefetch is browser-initiated, `std::nullopt` (for
  //   `referring_document_token`) is used.
  // - If the prefetch is embedder-initiated, `net::NetworkIsolationKey` of the
  //   embedder is used. Only used if `kPrefetchBrowserInitiatedTriggers` is
  //   enabeld. See crbug.com/40942681.
  //
  // For navigation, `std::optional<DocumentToken>` of the initiating document
  // of the navigation is used.
  //
  // See also the doc on crbug.com/40946257 for more context.
  class CONTENT_EXPORT Key {
   public:
    Key() = delete;
    Key(net::NetworkIsolationKey nik, GURL url);
    Key(std::optional<blink::DocumentToken> referring_document_token, GURL url);
    ~Key();

    // Movable and copyable.
    Key(Key&& other);
    Key& operator=(Key&& other);
    Key(const Key& other);
    Key& operator=(const Key& other);

    bool operator==(const Key& rhs) const = default;
    bool operator<(const Key& rhs) const {
      if (referring_document_token_or_nik_ !=
          rhs.referring_document_token_or_nik_) {
        return referring_document_token_or_nik_ <
               rhs.referring_document_token_or_nik_;
      }
      return url_ < rhs.url_;
    }

    const GURL& url() const { return url_; }

    Key WithNewUrl(const GURL& new_url) const {
      return absl::visit([&](const auto& e) { return Key(e, new_url); },
                         referring_document_token_or_nik_);
    }

    bool NonUrlPartIsSame(const Key& other) const {
      return referring_document_token_or_nik_ ==
             other.referring_document_token_or_nik_;
    }

   private:
    friend CONTENT_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                                   const Key& prefetch_key);

    absl::variant<std::optional<blink::DocumentToken>, net::NetworkIsolationKey>
        referring_document_token_or_nik_;
    GURL url_;
  };

  // Observer interface to listen to lifecycle events of `PrefetchContainer`.
  //
  // Each callback is called at most once in the lifecycle of a container.
  //
  // Be careful about using this. This is designed only for
  // `PrefetchMatchResolver2`.
  //
  // These callback are called only if `kPrefetchNewWaitLoop` is enabled.
  // Observer interface to listen to lifecycle events of `PrefetchContainer`.
  class Observer : public base::CheckedObserver {
   public:
    // Called at the head of dtor.
    //
    // TODO(crbug.com/356314759): Update the description to "Called just
    // before dtor is called."
    virtual void OnWillBeDestroyed(PrefetchContainer& prefetch_container) = 0;
    // Called if non-redirect header of prefetch response is determined, i.e.
    // successfully received or fetch requests including redirects failed.
    // Callers can check success/failure by `GetNonRedirectHead()`.
    virtual void OnDeterminedHead(PrefetchContainer& prefetch_container) = 0;
  };

  void OnWillBeDestroyed();

  const Key& key() const { return key_; }

  // The ID of the RenderFrameHost that triggered the prefetch.
  const GlobalRenderFrameHostId& GetReferringRenderFrameHostId() const {
    return referring_render_frame_host_id_;
  }
  bool HasSameReferringURLForMetrics(const PrefetchContainer& other) const;

  // The initial URL that was requested to be prefetched.
  const GURL& GetURL() const { return key_.url(); }

  // The current URL being fetched.
  GURL GetCurrentURL() const;

  // The previous URL, if this has been redirected. Invalid to call otherwise.
  GURL GetPreviousURL() const;

  // The type of this prefetch. Controls how the prefetch is handled.
  const PrefetchType& GetPrefetchType() const { return prefetch_type_; }

  // Whether this prefetch is initiated by renderer processes.
  // Currently this is equivalent to whether the trigger type is Speculation
  // Rules or not.
  bool IsRendererInitiated() const;

  // The origin and that initiates the prefetch request.
  const url::Origin& GetReferringOrigin() const { return referring_origin_; }

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
  void MakeResourceRequest(const net::HttpRequestHeaders& additional_headers);

  // Updates |referrer_| after a redirect.
  void UpdateReferrer(
      const GURL& new_referrer_url,
      const network::mojom::ReferrerPolicy& new_referrer_policy);

  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint() const {
    return no_vary_search_hint_;
  }

  base::WeakPtr<PrefetchContainer> GetWeakPtr() {
    return weak_method_factory_.GetWeakPtr();
  }

  // The status of the current prefetch. Note that |HasPrefetchStatus| will be
  // initially false until |SetPrefetchStatus| is called. |SetPrefetchStatus|
  // also sets |attempt_| PreloadingTriggeringOutcome and
  // PreloadingFailureReason. It is only safe to call after
  // `OnEligibilityCheckComplete`.
  void SetPrefetchStatus(PrefetchStatus prefetch_status);
  bool HasPrefetchStatus() const { return prefetch_status_.has_value(); }
  PrefetchStatus GetPrefetchStatus() const;

  // These are intended to be called on
  // PrefetchService::CheckAndSetPrefetchHoldbackStatus() to set this overridden
  // prefetch status to `attempt_`.
  bool HasOverriddenHoldbackStatus() const {
    return holdback_status_override_.has_value();
  }
  PreloadingHoldbackStatus GetOverriddenHoldbackStatus() const {
    CHECK(holdback_status_override_);
    return holdback_status_override_.value();
  }

  // The state enum of the current prefetch, to replace `PrefetchStatus`.
  // https://crbug.com/1494771
  // Design doc for PrefetchContainer state transitions:
  // https://docs.google.com/document/d/1dK4mAVoRrgTVTGdewthI_hA8AHirgXW8k6BmpK9gnBE/edit?usp=sharing
  enum class LoadState {
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

    // [Final state] Not heldback.
    //
    // On this state, refer to `PrefetchResponseReader`s for detailed
    // prefetching state and servability.
    //
    // Also, refer to `attempt_` for triggering outcome and failure reasons for
    // metrics.
    // `PreloadingAttempt::SetFailureReason()` can be only called on this state.
    // Note that these states of `attempt_` don't directly affect
    // `PrefetchResponseReader`'s servability.
    // (e.g. `PrefetchResponseReader::GetServableState()` can be still
    // `kServable` even if `attempt_` has a failure).
    kStarted,

    // [Final state] Heldback due to `PreloadingAttempt::ShouldHoldback()`.
    kFailedHeldback,
  };
  void SetLoadState(LoadState prefetch_status);
  LoadState GetLoadState() const;

  // Controls ownership of the |ProxyLookupClientImpl| used during the
  // eligibility check.
  void TakeProxyLookupClient(
      std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client);
  std::unique_ptr<ProxyLookupClientImpl> ReleaseProxyLookupClient();

  // Whether or not the prefetch was determined to be eligibile.
  void OnEligibilityCheckComplete(PreloadingEligibility eligibility);
  bool IsInitialPrefetchEligible() const;

  // Adds a the new URL to |redirect_chain_|.
  void AddRedirectHop(const net::RedirectInfo& redirect_info);

  // The length of the redirect chain for this prefetch.
  size_t GetRedirectChainSize() const { return redirect_chain_.size(); }

  // Whether this prefetch is a decoy. Decoy prefetches will not store the
  // response, and not serve any prefetched resources.
  void SetIsDecoy(bool is_decoy) { is_decoy_ = is_decoy; }
  bool IsDecoy() const { return is_decoy_; }

  // Whether this prefetch is potentially contaminated by cross-site state.
  // If so, it may need special handling for privacy.
  // See https://crbug.com/1439246.
  bool IsCrossSiteContaminated() const { return is_cross_site_contaminated_; }
  void MarkCrossSiteContaminated();

  // Allows for |PrefetchCookieListener|s to be reigsitered for
  // `GetCurrentSinglePrefetchToPrefetch()`.
  void RegisterCookieListener(network::mojom::CookieManager* cookie_manager);
  void StopAllCookieListeners();

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
  const base::WeakPtr<PrefetchStreamingURLLoader>& GetStreamingURLLoader()
      const;

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

  // The |PrefetchDocumentManager| that requested |this|.
  PrefetchDocumentManager* GetPrefetchDocumentManager() const;

  // Called when |PrefetchService::GetPrefetchToServe| and
  // |PrefetchService::ReturnPrefetchToServe| with |this|.
  void OnGetPrefetchToServe(bool blocked_until_head);
  void OnReturnPrefetchToServe(bool served, const GURL& navigated_url);

  // Returns whether or not this prefetch has been considered to serve for a
  // navigation in the past. If it has, then it shouldn't be used for any future
  // navigations.
  bool HasPrefetchBeenConsideredToServe() const;

  // Called when |PrefetchService::OnPrefetchComplete| is called for the
  // prefetch. This happens when |loader_| fully downloads the requested
  // resource.
  void OnPrefetchComplete(
      const network::URLLoaderCompletionStatus& completion_status);

  enum class ServableState {
    // Not servable nor should block until head received.
    kNotServable,

    // Servable.
    kServable,

    // |PrefetchService| should block until the head of |this| is
    // received on a navigation to a matching URL.
    kShouldBlockUntilHeadReceived,
  };

  // Note: Even if this returns `kServable`, `CreateRequestHandler()` can still
  // fail (returning null handler) due to final checks. See also the comment for
  // `PrefetchResponseReader::CreateRequestHandler()`.
  ServableState GetServableState(base::TimeDelta cacheable_duration) const;

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
  void OnDeterminedHead();
  void OnDeterminedHead2();
  // Unblocks waiting `PrefetchMatchResolver`.
  //
  // This method can be called multiple times.
  void UnblockPrefetchMatchResolver();

  void StartTimeoutTimer(base::TimeDelta timeout,
                         base::OnceClosure on_timeout_callback);

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

  // Sets DevTools observer
  void SetDevToolsObserver(
      base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer) {
    devtools_observer_ = std::move(devtools_observer);
  }

  // Returns DevTool observer
  const base::WeakPtr<SpeculationHostDevToolsObserver>& GetDevToolsObserver()
      const {
    return devtools_observer_;
  }

  const std::optional<PrefetchResponseSizes>& GetPrefetchResponseSizes() const {
    return prefetch_response_sizes_;
  }

  bool HasPreloadingAttempt() { return !!attempt_; }
  base::WeakPtr<PreloadingAttempt> preloading_attempt() { return attempt_; }

  // Simulates a prefetch container that has started its request. It sets the
  //`attempt_` to the correct state: `PreloadingEligibility::kEligible`,
  // `PreloadingHoldbackStatus::kAllowed` and
  // `PreloadingTriggeringOutcome::kReady`.
  void SimulateAttemptAtRequestStartForTest();
  // Simulates a prefetch container that reaches the interceptor. Similar to
  // |SimulateAttemptAtRequestStartForTest| but also marks the prefetch as
  // completed.
  void SimulateAttemptAtInterceptorForTest();
  void DisablePrecogLoggingForTest() { attempt_ = nullptr; }

  const std::optional<net::HttpNoVarySearchData>& GetNoVarySearchData() const {
    return no_vary_search_data_;
  }
  // Sets `no_vary_search_data_` from `GetHead()`. Exposed for tests.
  // RenderFrameHost is being used on no_vary_search::ProcessHead() to put
  // message to DevTools console and can be null.
  void MaybeSetNoVarySearchData(RenderFrameHost* rfh);

  // Called upon detecting a change to cookies within the redirect chain.
  //
  // Note that there are two paths:
  //
  // - Roughly speaking, when non-redirect header received and
  //   `PrefetchService`/`PrefetchContainer` detected cookies change of the head
  //   of redirect chain. `PrefetchMatchResolver`/`PrefetchMatchResolver2`
  //   propagates it to other waiting prefetches as they share domain.
  // - When `PrefetchURLLoaderInterceptor::MaybeCreateLoader()` handles
  //   redirects in the serving prefetch.
  void OnDetectedCookiesChange();
  void OnDetectedCookiesChange2();

  // Called when the prefetch request is started (i.e. the URL loader is created
  // & started).
  void OnPrefetchStarted();

  class SinglePrefetch;

  // A `Reader` represents the current state of serving.
  // The `Reader` methods all operate on the currently *serving*
  // `SinglePrefetch`, which is the element in |redirect_chain_| at index
  // |index_redirect_chain_to_serve_|.
  //
  // This works like `base::WeakPtr<PrefetchContainer>` plus additional states,
  // so check that the reader is valid (e.g. `if (reader)`) before calling other
  // methods (except for `Clone()`).
  //
  // TODO(crbug.com/40064891): Allow multiple Readers for a PrefetchContainer.
  // This might need ownership/lifetime changes of `Reader` and further cleaning
  // up the dependencies between `PrefetchContainer` and `Reader`.
  class CONTENT_EXPORT Reader final {
   public:
    Reader();

    Reader(base::WeakPtr<PrefetchContainer> prefetch_container,
           size_t index_redirect_chain_to_serve);

    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    Reader(Reader&&);
    Reader& operator=(Reader&&);

    ~Reader();

    PrefetchContainer* GetPrefetchContainer() const {
      return prefetch_container_.get();
    }
    Reader Clone() const;

    // Returns true if `this` is valid.
    // Do not call methods below if false.
    explicit operator bool() const { return GetPrefetchContainer(); }

    // Methods redirecting to `prefetch_container_`.
    PrefetchContainer::ServableState GetServableState(
        base::TimeDelta cacheable_duration) const;
    bool HasPrefetchStatus() const;
    PrefetchStatus GetPrefetchStatus() const;

    // Returns whether the Reader reached the end. If true, the methods below
    // shouldn't be called, because the current `SinglePrefetch` doesn't exist.
    bool IsEnd() const;

    // Whether or not an isolated network context is required to serve.
    bool IsIsolatedNetworkContextRequiredToServe() const;

    PrefetchNetworkContext* GetCurrentNetworkContextToServe() const;

    bool HaveDefaultContextCookiesChanged() const;

    // Before a prefetch can be served, any cookies added to the isolated
    // network context must be copied over to the default network context. These
    // functions are used to check and update the status of this process, as
    // well as record metrics about how long this process takes.
    bool HasIsolatedCookieCopyStarted() const;
    bool IsIsolatedCookieCopyInProgress() const;
    void OnIsolatedCookieCopyStart() const;
    void OnIsolatedCookiesReadCompleteAndWriteStart() const;
    void OnIsolatedCookieCopyComplete() const;
    void OnInterceptorCheckCookieCopy() const;
    void SetOnCookieCopyCompleteCallback(base::OnceClosure callback) const;

    // Called with the result of the probe. If the probing feature is enabled,
    // then a probe must complete successfully before the prefetch can be
    // served.
    void OnPrefetchProbeResult(PrefetchProbeResult probe_result) const;

    // Checks if the given URL matches the the URL that can be served next.
    bool DoesCurrentURLToServeMatch(const GURL& url) const;

    // Returns the URL that can be served next.
    const GURL& GetCurrentURLToServe() const;

    // Gets the current PrefetchResponseReader.
    base::WeakPtr<PrefetchResponseReader>
    GetCurrentResponseReaderToServeForTesting();

    // Called when one element of |redirect_chain_| is served and the next
    // element can now be served.
    void AdvanceCurrentURLToServe() { index_redirect_chain_to_serve_++; }

    // Returns the `SinglePrefetch` to be served next.
    const SinglePrefetch& GetCurrentSinglePrefetchToServe() const;

    // See the comment for `PrefetchResponseReader::CreateRequestHandler()`.
    PrefetchRequestHandler CreateRequestHandler();

    // See the corresponding functions on `PrefetchResponseReader`.
    // These apply to the current `SinglePrefetch` (and so, may change as the
    // prefetch advances through a redirect change).
    bool VariesOnCookieIndices() const;
    bool MatchesCookieIndices(
        base::span<const std::pair<std::string, std::string>> cookies) const;

   private:
    base::WeakPtr<PrefetchContainer> prefetch_container_;

    // The index of the element in |prefetch_container_.redirect_chain_| that
    // can be served.
    size_t index_redirect_chain_to_serve_ = 0;
  };

  Reader CreateReader();

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
  // `PrefetchMatchResolver2::candidates_`, i.e. those potentially matching
  // and expected to become servable at the head of
  // `PrefetchMatchResolver2::FindPrefetch()`.
  //
  // This can be called multiple times, because this can be called for multiple
  // `PrefetchMatchResolver2`s.
  void OnUnregisterCandidate(const GURL& navigated_url,
                             bool is_served,
                             std::optional<base::TimeDelta> blocked_duration);

  bool is_in_dtor() const { return is_in_dtor_; }

 protected:
  friend class PrefetchContainerTestBase;

  // Updates metrics based on the result of the prefetch request.
  void UpdatePrefetchRequestMetrics(
      const std::optional<network::URLLoaderCompletionStatus>&
          completion_status,
      const network::mojom::URLResponseHead* head);

 private:
  PrefetchContainer(
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
      bool is_javascript_enabled);

  // Update |prefetch_status_| and report prefetch status to
  // DevTools without updating TriggeringOutcome.
  void SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
      PrefetchStatus prefetch_status);

  // Add client hints headers to a request bound for |origin|.
  void AddClientHintsHeaders(const url::Origin& origin,
                             net::HttpRequestHeaders* request_headers);
  // Add X-Client-Data request header to a request.
  void AddXClientDataHeader(network::ResourceRequest& request);

  // Returns the `SinglePrefetch` to be prefetched next. This is the last
  // element in `redirect_chain_`, because, during prefetching from the network,
  // we push back `SinglePrefetch`s to `redirect_chain_` and access the latest
  // redirect hop.
  SinglePrefetch& GetCurrentSinglePrefetchToPrefetch() const;

  // Returns the `SinglePrefetch` for the redirect leg before
  // `GetCurrentSinglePrefetchToPrefetch()`. This must be called only if `this`
  // has redirect(s).
  const SinglePrefetch& GetPreviousSinglePrefetchToPrefetch() const;

  // Returns "Sec-Purpose" header value for a prefetch request to `request_url`.
  const char* GetSecPurposeHeaderValue(const GURL& request_url) const;

  // Called when a prefetch request could not be started because of eligibility
  // reasons. Should only be called for the initial prefetch request and not
  // redirects.
  void OnInitialPrefetchFailedIneligible(PreloadingEligibility eligibility);

  // Returns the |PrefetchStartResultCode| based on the |eligibility|.
  PrefetchStartResultCode GetPrefetchFailedIneligibleStartResultCode(
      PreloadingEligibility eligibility);

  // The ID of the RenderFrameHost/Document that triggered the prefetch.
  // This will be empty when browser-initiated prefetch.
  const GlobalRenderFrameHostId referring_render_frame_host_id_;

  // The origin and URL that initiates the prefetch request.
  // For renderer-initiated prefetch, this is calculated by referring
  // RenderFrameHost's LastCommittedOrigin. For browser-initiated prefetch, this
  // is sometimes explicitly passed via ctor, otherwise opaque origin.
  const url::Origin referring_origin_;
  // Used by metrics for equality checks, only works for renderer-initiated
  // triggers.
  const std::optional<size_t> referring_url_hash_;

  // The key used to match this PrefetchContainer, including the URL that was
  // requested to prefetch.
  const PrefetchContainer::Key key_;

  // The type of this prefetch. This controls some specific details about how
  // the prefetch is handled, including whether an isolated network context or
  // the default network context is used to perform the prefetch, whether or
  // not the preftch proxy is used, and whether or not subresources are
  // prefetched.
  PrefetchType prefetch_type_;

  // The referrer to use for the request.
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

  // The No-Vary-Search hint of the prefetch, which is specified by the
  // speculation rules and can be different from actual `no_vary_search_data_`.
  const std::optional<net::HttpNoVarySearchData> no_vary_search_hint_;

  // The |PrefetchDocumentManager| that requested |this|.
  // This will be nullptr when the prefetch is initiated by browser.
  base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager_;

  // The |BrowserContext| in which this is being run.
  base::WeakPtr<BrowserContext> browser_context_;

  // The current status, if any, of the prefetch.
  // TODO(crbug.com/40075414): Use `load_state_` instead for non-metrics
  // purpose.
  std::optional<PrefetchStatus> prefetch_status_;

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
  std::vector<std::unique_ptr<SinglePrefetch>> redirect_chain_;

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

  ukm::SourceId ukm_source_id_;

  // The sizes information of the prefetched response.
  std::optional<PrefetchResponseSizes> prefetch_response_sizes_;

  // The amount  of time it took for the prefetch to complete.
  std::optional<base::TimeDelta> fetch_duration_;

  // The amount  of time it took for the headers to be received.
  std::optional<base::TimeDelta> header_latency_;

  // Whether or not a navigation to this prefetch occurred.
  bool navigated_to_ = false;

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

  // Weak pointer to DevTools observer
  base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer_;

  // `PreloadingAttempt` is used to track the lifecycle of the preloading event,
  // and reports various statuses to UKM dashboard. It is initialised along with
  // `this`, and destroyed when `WCO::DidFinishNavigation` is fired.
  // `attempt_`'s eligibility is set in `OnEligibilityCheckComplete`, and its
  // holdback status, triggering outcome and failure reason are set in
  // `SetPrefetchStatus`.
  base::WeakPtr<PreloadingAttempt> attempt_;

  // If set, this value is used to override holdback status derived by the
  // normal process. It is set to `attempt_` on
  // PrefetchService::CheckAndSetPrefetchHoldbackStatus().
  std::optional<PreloadingHoldbackStatus> holdback_status_override_ =
      std::nullopt;

  // A DevTools token used to identify initiator document if the prefetch is
  // triggered by SpeculationRules.
  std::optional<base::UnguessableToken> initiator_devtools_navigation_token_ =
      std::nullopt;

  // The time at which |PrefetchService| started blocking until the head of
  // |this| was received.
  std::optional<base::TimeTicks> blocked_until_head_start_time_;

  // A timer used to limit the maximum amount of time that a navigation can be
  // blocked waiting for the head of this prefetch to be received.
  std::unique_ptr<base::OneShotTimer> block_until_head_timer_;

  // Callback for non-blocking call `StartBlockUntilHead()`.
  //
  // TODO(crbug.com/353490734): Remove it.
  base::OnceCallback<void(PrefetchContainer&)>
      on_maybe_determined_head_callback_;

  // Browser callbacks.
  std::optional<PrefetchStartCallback> prefetch_start_callback_;

  std::unique_ptr<base::OneShotTimer> timeout_timer_;

  // Whether JavaScript is on in this contents (or was, when this prefetch
  // started). This affects Client Hints behavior. Per-origin settings are
  // handled later, according to
  // |ClientHintsControllerDelegate::IsJavaScriptAllowed|.
  bool is_javascript_enabled_ = false;

  // True iff the destructor was called.
  bool is_in_dtor_ = false;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PrefetchContainer> weak_method_factory_{this};
};

// For debug logs.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& ostream,
    const PrefetchContainer& prefetch_container);

CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& ostream,
    const PrefetchContainer::Key& prefetch_key);

CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& ostream,
    PrefetchContainer::ServableState servable_state);

CONTENT_EXPORT std::ostream& operator<<(std::ostream& ostream,
                                        PrefetchContainer::LoadState state);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
