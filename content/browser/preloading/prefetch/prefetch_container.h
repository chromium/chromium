// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_

#include <utility>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/speculation_host_devtools_observer.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "net/http/http_no_vary_search_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
namespace mojom {
class CookieManager;
}  // namespace mojom
}  // namespace network

namespace content {

class PrefetchCookieListener;
class PrefetchDocumentManager;
class PrefetchNetworkContext;
class PrefetchService;
class PrefetchServingPageMetricsContainer;
class PrefetchStreamingURLLoader;
class PreloadingAttempt;
class ProxyLookupClientImpl;

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
// single redirect hop, while a single `PrefetchStreamingURLLoader` in
// `streaming_loaders_` can receive multiple redirect hops unless network
// context switching is needed.
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
// - `PrefetchService::OnGotEligibilityResultForRedirect()` [redirect].
// A new `PrefetchStreamingURLLoader` is also created if needed in
// `PrefetchService::MakePrefetchRequest()`.
class CONTENT_EXPORT PrefetchContainer {
 public:
  PrefetchContainer(
      const GlobalRenderFrameHostId& referring_render_frame_host_id,
      const GURL& url,
      const PrefetchType& prefetch_type,
      const blink::mojom::Referrer& referrer,
      absl::optional<net::HttpNoVarySearchData> no_vary_search_expected,
      blink::mojom::SpeculationInjectionWorld world,
      base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager);
  ~PrefetchContainer();

  PrefetchContainer(const PrefetchContainer&) = delete;
  PrefetchContainer& operator=(const PrefetchContainer&) = delete;

  // Defines the key to uniquely identify a prefetch.
  using Key = std::pair<GlobalRenderFrameHostId, GURL>;
  Key GetPrefetchContainerKey() const {
    return std::make_pair(referring_render_frame_host_id_, prefetch_url_);
  }

  // The ID of the RenderFrameHost that triggered the prefetch.
  GlobalRenderFrameHostId GetReferringRenderFrameHostId() const {
    return referring_render_frame_host_id_;
  }

  // The initial URL that was requested to be prefetched.
  GURL GetURL() const { return prefetch_url_; }

  // The current URL being fetched.
  GURL GetCurrentURL() const;

  // The previous URL, if this has been redirected. Invalid to call otherwise.
  GURL GetPreviousURL() const;

  // The type of this prefetch. Controls how the prefetch is handled.
  const PrefetchType& GetPrefetchType() const { return prefetch_type_; }

  // Whether or not an isolated network context is required to the next
  // prefetch.
  bool IsIsolatedNetworkContextRequiredForCurrentPrefetch() const;

  // Whether or not an isolated network context is required for the previous
  // redirect hop of the given url.
  bool IsIsolatedNetworkContextRequiredForPreviousRedirectHop() const;

  base::WeakPtr<PrefetchResponseReader> GetResponseReaderForCurrentPrefetch();

  // Gets the site for the previous redirect hop to the given URL.
  net::SchemefulSite GetSiteForPreviousRedirectHop(const GURL& url) const;

  // Whether or not the prefetch proxy would be required to fetch the given url
  // based on |prefetch_type_|.
  bool IsProxyRequiredForURL(const GURL& url) const;

  const blink::mojom::Referrer& GetReferrer() const { return referrer_; }

  const network::ResourceRequest* GetResourceRequest() const {
    return resource_request_.get();
  }
  void MakeResourceRequest(const net::HttpRequestHeaders& additional_headers);

  // Updates |referrer_| after a redirect.
  void UpdateReferrer(
      const GURL& new_referrer_url,
      const network::mojom::ReferrerPolicy& new_referrer_policy);

  const net::SchemefulSite& GetReferringSite() const { return referring_site_; }

  const absl::optional<net::HttpNoVarySearchData>& GetNoVarySearchHint() const {
    return no_vary_search_hint_;
  }

  base::WeakPtr<PrefetchContainer> GetWeakPtr() {
    return weak_method_factory_.GetWeakPtr();
  }

  // The status of the current prefetch. Note that |HasPrefetchStatus| will be
  // initially false until |SetPrefetchStatus| is called. |SetPrefetchStatus|
  // also sets |attempt_| PreloadingHoldbackStatus, PreloadingTriggeringOutcome
  // and PreloadingFailureReason. It is only safe to call after
  // `OnEligibilityCheckComplete`.
  void SetPrefetchStatus(PrefetchStatus prefetch_status);
  bool HasPrefetchStatus() const { return prefetch_status_.has_value(); }
  PrefetchStatus GetPrefetchStatus() const;

  // Controls ownership of the |ProxyLookupClientImpl| used during the
  // eligibility check.
  void TakeProxyLookupClient(
      std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client);
  std::unique_ptr<ProxyLookupClientImpl> ReleaseProxyLookupClient();

  // Whether or not the prefetch was determined to be eligibile.
  void OnEligibilityCheckComplete(bool is_eligible,
                                  absl::optional<PrefetchStatus> status);
  bool IsInitialPrefetchEligible() const;

  // Adds a the new URL to |redirect_chain_|.
  void AddRedirectHop(const net::RedirectInfo& redirect_info);

  // The length of the redirect chain for this prefetch.
  size_t GetRedirectChainSize() const { return redirect_chain_.size(); }

  // Whether this prefetch is a decoy. Decoy prefetches will not store the
  // response, and not serve any prefetched resources.
  void SetIsDecoy(bool is_decoy) { is_decoy_ = is_decoy; }
  bool IsDecoy() const { return is_decoy_; }

  // Allows for |PrefetchCookieListener|s to be reigsitered for
  // `GetCurrentSinglePrefetchToPrefetch()`.
  void RegisterCookieListener(network::mojom::CookieManager* cookie_manager);
  void StopAllCookieListeners();

  // The network context used to make network requests for the next prefetch.
  PrefetchNetworkContext* GetOrCreateNetworkContextForCurrentPrefetch(
      PrefetchService* prefetch_service);

  // Closes idle connections for all elements in |network_contexts_|.
  void CloseIdleConnections();

  // Adds the given |PrefetchStreamingURLLoader| to the end of
  // |streaming_loaders_|.
  void TakeStreamingURLLoader(
      std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader);

  bool HasStreamingURLLoadersForTest() const;

  // Returns the last |PrefetchStreamingURLLoader| from |streaming_loaders_|,
  // i.e. the URL loader being used for prefetching the current redirect hop.
  // This method should be used during prefetching and shouldn't be called for
  // serving purpose.
  //
  // TODO(https://crbug.com/1449360): Migrate callers (e.g. to
  // GetNonRedirectResponseReader()) that don't meet this criteria.
  PrefetchStreamingURLLoader* GetLastStreamingURLLoader() const;

  // Returns the PrefetchResponseReader corresponding to the last non-redirect
  // response, if already received its head, or otherwise nullptr.
  const PrefetchResponseReader* GetNonRedirectResponseReader() const;

  // Clears all |PrefetchStreamingURLLoader|s and |PrefetchResponseReader|s from
  // |streaming_loaders_|.
  void ResetAllStreamingURLLoaders();

  // The |PrefetchDocumentManager| that requested |this|.
  PrefetchDocumentManager* GetPrefetchDocumentManager() const;

  // Called when |PrefetchService::GetPrefetchToServe| and
  // |PrefetchService::ReturnPrefetchToServe| with |this|.
  void OnGetPrefetchToServe(bool blocked_until_head);
  void OnReturnPrefetchToServe(bool served);

  // Returns whether or not this prefetch has been considered to serve for a
  // navigation in the past. If it has, then it shouldn't be used for any future
  // navigations.
  bool HasPrefetchBeenConsideredToServe() const { return navigated_to_; }

  // Called when |PrefetchService::OnPrefetchComplete| is called for the
  // prefetch. This happens when |loader_| fully downloads the requested
  // resource.
  void OnPrefetchComplete();

  // Whether or not |PrefetchService| should block until the head of |this| is
  // received on a navigation to a matching URL.
  bool ShouldBlockUntilHeadReceived() const;

  // Allows for a timer to be used to limit the maximum amount of time that a
  // navigation can be blocked waiting for the head of this prefetch to be
  // received.
  void TakeBlockUntilHeadTimer(
      std::unique_ptr<base::OneShotTimer> block_until_head_timer);
  void ResetBlockUntilHeadTimer();

  // Whether or not |this| is servable.
  bool IsPrefetchServable(base::TimeDelta cacheable_duration) const;

  // Called once it is determined whether or not the prefetch is servable, i.e.
  // either when non-redirect response head is received, or when determined not
  // servable.
  void OnReceivedHead();
  void SetOnReceivedHeadCallback(base::OnceClosure on_received_head_callback);
  base::OnceClosure ReleaseOnReceivedHeadCallback();

  // Returns the head of the prefetched response. If there is no valid response,
  // then returns null.
  const network::mojom::URLResponseHead* GetHead();

  // Returns the time between the prefetch request was sent and the time the
  // response headers were received. Not set if the prefetch request hasn't been
  // sent or the response headers haven't arrived.
  absl::optional<base::TimeDelta> GetPrefetchHeaderLatency() const {
    return header_latency_;
  }

  // Allow for the serving page to metrics when changes to the prefetch occur.
  void SetServingPageMetrics(base::WeakPtr<PrefetchServingPageMetricsContainer>
                                 serving_page_metrics_container);
  void UpdateServingPageMetrics();

  // Returns request id to be used by DevTools
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

  const absl::optional<PrefetchResponseSizes>& GetPrefetchResponseSizes()
      const {
    return prefetch_response_sizes_;
  }

  bool HasPreloadingAttempt() { return !!attempt_; }
  base::WeakPtr<PreloadingAttempt> preloading_attempt() { return attempt_; }

  // Simulates a prefetch container that reaches the interceptor. It sets the
  // `attempt_` to the correct state: `PreloadingEligibility::kEligible`,
  // `PreloadingHoldbackStatus::kAllowed` and
  // `PreloadingTriggeringOutcome::kReady`.
  void SimulateAttemptAtInterceptorForTest();
  void DisablePrecogLoggingForTest() { attempt_ = nullptr; }

  const absl::optional<net::HttpNoVarySearchData>& GetNoVarySearchData() const {
    return no_vary_search_data_;
  }
  void SetNoVarySearchData(net::HttpNoVarySearchData no_vary_search_data) {
    no_vary_search_data_ = std::move(no_vary_search_data);
  }

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
  // TODO(crbug.com/1449360): Allow multiple Readers for a PrefetchContainer.
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
    bool IsPrefetchServable(base::TimeDelta cacheable_duration) const;
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

    // Set up a RequestHandler from the Reader. After this point,
    // - The PrefetchResponseReader will manage its own lifetime, and will
    // delete itself once its serving client is finished.
    // - If IsReadyToServeLastEvents() is true, the PrefetchStreamingURLLoader
    // will manage its own lifetime, and will delete itself once its prefetching
    // request is finished. Otherwise, PrefetchStreamingURLLoader is kept owned
    // by `streaming_loaders_`.
    PrefetchResponseReader::RequestHandler CreateRequestHandler();

   private:
    base::WeakPtr<PrefetchContainer> prefetch_container_;

    // The index of the element in |prefetch_container_.redirect_chain_| that
    // can be served.
    size_t index_redirect_chain_to_serve_ = 0;
  };

  Reader CreateReader();

 protected:
  friend class PrefetchContainerTest;

  // Updates metrics based on the result of the prefetch request.
  void UpdatePrefetchRequestMetrics(
      const absl::optional<network::URLLoaderCompletionStatus>&
          completion_status,
      const network::mojom::URLResponseHead* head);

 private:
  // Update |prefetch_status_| and report prefetch status to
  // DevTools without updating TriggeringOutcome.
  void SetPrefetchStatusWithoutUpdatingTriggeringOutcome(
      PrefetchStatus prefetch_status);

  // Returns the `SinglePrefetch` to be prefetched next. This is the last
  // element in `redirect_chain_`, because, during prefetching from the network,
  // we push back `SinglePrefetch`s to `redirect_chain_` and access the latest
  // redirect hop.
  SinglePrefetch& GetCurrentSinglePrefetchToPrefetch() const;

  // Returns the `SinglePrefetch` for the redirect leg before
  // `GetCurrentSinglePrefetchToPrefetch()`. This must be called only if `this`
  // has redirect(s).
  const SinglePrefetch& GetPreviousSinglePrefetchToPrefetch() const;

  PrefetchResponseReader::RequestHandler CreateRequestHandlerInternal(
      Reader& reader);

  // The ID of the RenderFrameHost that triggered the prefetch.
  GlobalRenderFrameHostId referring_render_frame_host_id_;

  // The URL that was requested to be prefetch.
  const GURL prefetch_url_;

  // The type of this prefetch. This controls some specific details about how
  // the prefetch is handled, including whether an isolated network context or
  // the default network context is used to perform the prefetch, whether or
  // not the preftch proxy is used, and whether or not subresources are
  // prefetched.
  PrefetchType prefetch_type_;

  // The referrer to use for the request.
  blink::mojom::Referrer referrer_;

  // The origin and site of the page that requested the prefetched.
  url::Origin referring_origin_;
  net::SchemefulSite referring_site_;

  // Information about the current prefetch request. Updated when a redirect is
  // encountered, whether or not the direct can be processed by the same URL
  // loader or requires the instantiation of a new loader.
  std::unique_ptr<network::ResourceRequest> resource_request_;

  // The No-Vary-Search response data, parsed from the actual response header
  // (`GetHead()`).
  absl::optional<net::HttpNoVarySearchData> no_vary_search_data_;

  // The No-Vary-Search hint of the prefetch, which is specified by the
  // speculation rules and can be different from actual `no_vary_search_data_`.
  const absl::optional<net::HttpNoVarySearchData> no_vary_search_hint_;

  // The |PrefetchDocumentManager| that requested |this|. Initially it owns
  // |this|, but once the network request for the prefetch is started,
  // ownernship is transferred to |PrefetchService|.
  base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager_;

  // The current status, if any, of the prefetch.
  absl::optional<PrefetchStatus> prefetch_status_;

  // Looks up the proxy settings in the default network context all URLs in
  // |redirect_chain_|.
  std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client_;

  // Whether this prefetch is a decoy or not. If the prefetch is a decoy then
  // any prefetched resources will not be served.
  bool is_decoy_ = false;

  // The redirect chain resulting from prefetching |prefetch_url_|.
  std::vector<std::unique_ptr<SinglePrefetch>> redirect_chain_;

  // The network contexts used for this prefetch. They key corresponds to the
  // |is_isolated_network_context_required| param of the
  // |PrefetchNetworkContext|.
  std::map<bool, std::unique_ptr<PrefetchNetworkContext>> network_contexts_;

  // The series of streaming URL loaders used to fetch and serve this prefetch.
  // Multiple streaming URL loaders are used in the event a redirect causes a
  // change in the network context.
  std::vector<std::unique_ptr<PrefetchStreamingURLLoader>> streaming_loaders_;

  // The time at which |prefetched_response_| was received. This is used to
  // determine whether or not |prefetched_response_| is stale.
  absl::optional<base::TimeTicks> prefetch_received_time_;

  ukm::SourceId ukm_source_id_;

  // The sizes information of the prefetched response.
  absl::optional<PrefetchResponseSizes> prefetch_response_sizes_;

  // The amount  of time it took for the prefetch to complete.
  absl::optional<base::TimeDelta> fetch_duration_;

  // The amount  of time it took for the headers to be received.
  absl::optional<base::TimeDelta> header_latency_;

  // Whether or not a navigation to this prefetch occurred.
  bool navigated_to_ = false;

  // The result of probe when checked on navigation.
  absl::optional<PrefetchProbeResult> probe_result_;

  // Reference to metrics related to the page that considered using this
  // prefetch.
  base::WeakPtr<PrefetchServingPageMetricsContainer>
      serving_page_metrics_container_;

  // Request identifier used by DevTools
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

  // A DevTools token used to identify initiator document if the prefetch is
  // triggered by SpeculationRules.
  absl::optional<base::UnguessableToken> initiator_devtools_navigation_token_ =
      absl::nullopt;

  // The time at which |PrefetchService| started blocking until the head of
  // |this| was received.
  absl::optional<base::TimeTicks> blocked_until_head_start_time_;

  // A timer used to limit the maximum amount of time that a navigation can be
  // blocked waiting for the head of this prefetch to be received.
  std::unique_ptr<base::OneShotTimer> block_until_head_timer_;

  // Called when `OnReceivedHead()` is called.
  base::OnceClosure on_received_head_callback_;

  base::WeakPtrFactory<PrefetchContainer> weak_method_factory_{this};
};

// For debug logs.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& ostream,
    const PrefetchContainer& prefetch_container);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
