// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_

#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/no_vary_search_helper.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
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

  // The type of this prefetch. Controls how the prefetch is handled.
  const PrefetchType& GetPrefetchType() const { return prefetch_type_; }

  // Whether or not an isolated network context is required to fetch the given
  // url.
  bool IsIsolatedNetworkContextRequiredForURL(const GURL& url) const;

  // Whether or not an isolated network context is required for the previous
  // redirect hop of the given url.
  bool IsIsolatedNetworkContextRequiredForPreviousRedirectHop(
      const GURL& url) const;

  // Whether or not the prefetch proxy would be required to fetch the given url
  // based on |prefetch_type_|.
  bool IsProxyRequiredForURL(const GURL& url) const;

  const blink::mojom::Referrer& GetReferrer() const { return referrer_; }

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
  void OnEligibilityCheckComplete(const GURL& url,
                                  bool is_eligible,
                                  absl::optional<PrefetchStatus> status);
  bool IsInitialPrefetchEligible() const;

  // Adds a the new URL to |redirect_chain_|.
  void AddRedirectHop(const GURL& url);

  // Gets the result of the eligibility check for the given URL. The URL must be
  // in |redirect_chain_|. A value of absl::nullopt indicates that the
  // eligibility check is still in progress.
  absl::optional<bool> GetEligibilityResultForRedirect(const GURL& url);

  // The length of the redirect chain for this prefetch.
  size_t GetRedirectChainSize() const { return redirect_chain_.size(); }
  GURL GetMatchingURLFromRedirectChain() const;

  // Whether this prefetch is a decoy. Decoy prefetches will not store the
  // response, and not serve any prefetched resources.
  void SetIsDecoy(bool is_decoy) { is_decoy_ = is_decoy; }
  bool IsDecoy() const { return is_decoy_; }

  // Allows for |PrefetchCookieListener|s to be reigsitered for elements of
  // |redirect_chain_|.
  void RegisterCookieListener(const GURL& url,
                              network::mojom::CookieManager* cookie_manager);
  void StopAllCookieListeners();
  bool HaveDefaultContextCookiesChanged(const GURL& url) const;

  // Before a prefetch can be served, any cookies added to the isolated network
  // context must be copied over to the default network context. These functions
  // are used to check and update the status of this process, as well as record
  // metrics about how long this process takes. These functions all operate on
  // the element in |redirect_chain_| at index
  // |index_redirect_chain_to_serve_|.
  bool HasIsolatedCookieCopyStarted() const;
  bool IsIsolatedCookieCopyInProgress() const;
  void OnIsolatedCookieCopyStart();
  void OnIsolatedCookiesReadCompleteAndWriteStart();
  void OnIsolatedCookieCopyComplete();
  void OnInterceptorCheckCookieCopy();
  void SetOnCookieCopyCompleteCallback(base::OnceClosure callback);

  // The network context used to make network requests for the given URL within
  // this prefetch.
  PrefetchNetworkContext* GetOrCreateNetworkContextForURL(
      const GURL& url,
      PrefetchService* prefetch_service);
  PrefetchNetworkContext* GetNetworkContextForURL(const GURL& url) const;

  // Closes idle connections for all elements in |network_contexts_|.
  void CloseIdleConnections();

  // Adds the given |PrefetchStreamingURLLoader| to the end of
  // |streaming_loaders_|.
  void TakeStreamingURLLoader(
      std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader);

  // Returns the first |PrefetchStreamingURLLoader| from |streaming_loaders_|.
  // This URL loader should be used when serving the prefetch.
  PrefetchStreamingURLLoader* GetFirstStreamingURLLoader() const;

  // Removes the first |PrefetchStreamingURLLoader| from |streaming_loaders_|
  // and gives owernship of it to the caller.
  std::unique_ptr<PrefetchStreamingURLLoader> ReleaseFirstStreamingURLLoader();

  // Returns the last |PrefetchStreamingURLLoader| from |streaming_loaders_|.
  // This URL loader should be used when fetching the prefetch.
  PrefetchStreamingURLLoader* GetLastStreamingURLLoader() const;

  // Clears all |PrefetchStreamingURLLoader|s from |streaming_loaders_|.
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

  // Called with the result of the probe. If the probing feature is enabled,
  // then a probe must complete successfully before the prefetch can be served.
  void OnPrefetchProbeResult(PrefetchProbeResult probe_result);

  // Called when |PrefetchService::OnPrefetchComplete| is called for the
  // prefetch. This happens when |loader_| fully downloads the requested
  // resource.
  void OnPrefetchComplete();

  // Whether or not |PrefetchService| should block until the head of |this| is
  // received on a navigation to a matching URL.
  bool ShouldBlockUntilHeadReceived() const;

  // Whether or not |this| is servable.
  bool IsPrefetchServable(base::TimeDelta cacheable_duration) const;

  // Checks if the given URL matches the element in |redirect_chain_| at index
  // |index_redirect_chain_to_serve_|.
  bool DoesCurrentURLToServeMatch(const GURL& url) const;

  // Returns the URL that can be served next. This is the url of the element in
  // |redirect_chain_| at index |index_redirect_chain_to_serve_|.
  const GURL& GetCurrentURLToServe() const;

  // Called when one element of |redirect_chain_| is served and the next element
  // can now be served.
  void AdvanceCurrentURLToServe() { index_redirect_chain_to_serve_++; }

  // Called when |this| has received prefetched response's head.
  // Once this is called, we should be able to call GetHead() and receive a
  // non-null result.
  void OnPrefetchedResponseHeadReceived();

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

  void SetNoVarySearchHelper(
      scoped_refptr<NoVarySearchHelper> no_vary_search_helper) {
    no_vary_search_helper_ = no_vary_search_helper;
  }

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

  // Holds the state for the request for a single URL in the context of the
  // broader prefetch. A prefetch can request multiple URLs due to redirects.
  class SinglePrefetch {
   public:
    explicit SinglePrefetch(const GURL& url,
                            const net::SchemefulSite& referring_site);
    ~SinglePrefetch();

    SinglePrefetch(const SinglePrefetch&) = delete;
    SinglePrefetch& operator=(const SinglePrefetch&) = delete;

    // The URL that will potentially be prefetched. This can be the original
    // prefetch URL, or a URL from a redirect resulting from requesting the
    // original prefetch URL.
    GURL url_;

    bool is_isolated_network_context_required_;

    // Whether this |url_| is eligible to be prefetched
    absl::optional<bool> is_eligible_;

    // This tracks whether the cookies associated with |url_| have changed at
    // some point after the initial eligibility check.
    std::unique_ptr<PrefetchCookieListener> cookie_listener_;

    // The different possible states of the cookie copy process.
    enum class CookieCopyStatus {
      kNotStarted,
      kInProgress,
      kCompleted,
    };

    // The current state of the cookie copy process for this prefetch.
    CookieCopyStatus cookie_copy_status_ = CookieCopyStatus::kNotStarted;

    // The timestamps of when the overall cookie copy process starts, and midway
    // when the cookies are read from the isolated network context and are about
    // to be written to the default network context.
    absl::optional<base::TimeTicks> cookie_copy_start_time_;
    absl::optional<base::TimeTicks> cookie_read_end_and_write_start_time_;

    // A callback that runs once |cookie_copy_status_| is set to |kCompleted|.
    base::OnceClosure on_cookie_copy_complete_callback_;
  };

  // Helper function to get the |SinglePrefetch| for the given URL.
  SinglePrefetch* GetSinglePrefetch(const GURL& url) const;

  // Helper function go get the |SinglePrefetch| that preceded the given URL.
  // If called on the original URL of the prefetch, then nullptr is returned.
  SinglePrefetch* GetPreviousSinglePrefetch(const GURL& url) const;

  // Helper function to match URLs either directly or using
  // |no_vary_search_helper_|.
  bool IsMatchingURL(const GURL& internal_url, const GURL& external_url) const;

  // The ID of the RenderFrameHost that triggered the prefetch.
  GlobalRenderFrameHostId referring_render_frame_host_id_;

  // The URL that was requested to be prefetch.
  GURL prefetch_url_;

  // The type of this prefetch. This controls some specific details about how
  // the prefetch is handled, including whether an isolated network context or
  // the default network context is used to perform the prefetch, whether or
  // not the preftch proxy is used, and whether or not subresources are
  // prefetched.
  PrefetchType prefetch_type_;

  // The referrer to use for the request.
  const blink::mojom::Referrer referrer_;

  // The origin and site of the page that requested the prefetched.
  url::Origin referring_origin_;
  net::SchemefulSite referring_site_;

  // The No-Vary-Search hint of the prefetch.
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

  // The index of the element in |redirect_chain_| that can be served.
  size_t index_redirect_chain_to_serve_ = 0;

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

  // Used to match URLs based on no vary search params.
  scoped_refptr<NoVarySearchHelper> no_vary_search_helper_;

  // A DevTools token used to identify initiator document if the prefetch is
  // triggered by SpeculationRules.
  absl::optional<base::UnguessableToken> initiator_devtools_navigation_token_ =
      absl::nullopt;

  // The time at which |PrefetchService| started blocking until the head of
  // |this| was received.
  absl::optional<base::TimeTicks> blocked_until_head_start_time_;

  base::WeakPtrFactory<PrefetchContainer> weak_method_factory_{this};
};

// For debug logs.
CONTENT_EXPORT std::ostream& operator<<(
    std::ostream& ostream,
    const PrefetchContainer& prefetch_container);

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
