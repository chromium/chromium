// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_H_

#include <map>

#include "base/dcheck_is_on.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_context.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network::mojom {
class NetworkContext;
class URLLoaderFactory;
}  // namespace network::mojom

namespace content {

class BrowserContext;
class PrefetchMatchResolver;
class PrefetchOriginProber;
class PrefetchProxyConfigurator;
class PrefetchServiceDelegate;
class ServiceWorkerContext;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrefetchRedirectResult {
  kSuccessRedirectFollowed = 0,
  kFailedNullPrefetch = 1,
  kFailedRedirectsDisabled = 2,
  kFailedInvalidMethod = 3,
  kFailedInvalidResponseCode = 4,
  kFailedInvalidChangeInNetworkContext = 5,
  kFailedIneligible = 6,
  kFailedInsufficientReferrerPolicy = 7,
  kMaxValue = kFailedInsufficientReferrerPolicy,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrefetchRedirectNetworkContextTransition {
  kDefaultToDefault = 0,
  kDefaultToIsolated = 1,
  kIsolatedToDefault = 2,
  kIsolatedToIsolated = 3,
  kMaxValue = kIsolatedToIsolated,
};

// Manages all prefetches within a single BrowserContext. Responsible for
// checking the eligibility of the prefetch, making the network request for the
// prefetch, and provide prefetched resources to URL loader interceptor when
// needed.
class CONTENT_EXPORT PrefetchService {
 public:
  static PrefetchService* GetFromFrameTreeNodeId(int frame_tree_node_id);
  static void SetFromFrameTreeNodeIdForTesting(
      int frame_tree_node_id,
      std::unique_ptr<PrefetchService> prefetch_service);

  // |browser_context| must outlive this instance. In general this should always
  // be true, since |PrefetchService| will be indirectly owned by
  // |BrowserContext|.
  explicit PrefetchService(BrowserContext* browser_context);
  virtual ~PrefetchService();

  PrefetchService(const PrefetchService&) = delete;
  const PrefetchService& operator=(const PrefetchService&) = delete;

  BrowserContext* GetBrowserContext() const { return browser_context_; }

  PrefetchServiceDelegate* GetPrefetchServiceDelegate() const {
    return delegate_.get();
  }

  PrefetchProxyConfigurator* GetPrefetchProxyConfigurator() const {
    return prefetch_proxy_configurator_.get();
  }

  virtual PrefetchOriginProber* GetPrefetchOriginProber() const;
  virtual void PrefetchUrl(base::WeakPtr<PrefetchContainer> prefetch_container);

  // Finds the prefetch (if any) that can be used to serve a navigation to
  // |url|, and then calls |on_prefetch_to_serve_ready| with that prefetch.
  using OnPrefetchToServeReady =
      base::OnceCallback<void(PrefetchContainer::Reader prefetch_to_serve)>;
  void GetPrefetchToServe(const PrefetchContainer::Key& key,
                          base::WeakPtr<PrefetchServingPageMetricsContainer>
                              serving_page_metrics_container,
                          PrefetchMatchResolver& prefetch_match_resolver);

  // Copies any cookies in the isolated network context associated with
  // |prefetch_container| to the default network context.
  virtual void CopyIsolatedCookies(const PrefetchContainer::Reader& reader);

  void AddPrefetchContainer(
      std::unique_ptr<PrefetchContainer> prefetch_container);

  void ResetPrefetch(base::WeakPtr<PrefetchContainer> prefetch_container);

  // Called by PrefetchDocumentManager when it finishes processing the latest
  // update of speculation candidates.
  void OnCandidatesUpdated();

  // Helper functions to control the behavior of the eligibility check when
  // testing.
  static void SetServiceWorkerContextForTesting(ServiceWorkerContext* context);
  static void SetHostNonUniqueFilterForTesting(
      bool (*filter)(base::StringPiece));

  // Sets the URLLoaderFactory to be used during testing instead of the
  // |PrefetchNetworkContext| associated with each |PrefetchContainer|. Note
  // that this does not take ownership of |url_loader_factory|, and caller must
  // keep ownership over the course of the test.
  static void SetURLLoaderFactoryForTesting(
      network::mojom::URLLoaderFactory* url_loader_factory);

  // Sets the NetworkContext to use just for the proxy lookup. Note that this
  // does not take ownership of |network_context|, and the caller must keep
  // ownership over the course of the test.
  static void SetNetworkContextForProxyLookupForTesting(
      network::mojom::NetworkContext* network_context);

  // Set a callback for waiting for prefetch completion in tests.
  using OnPrefetchResponseCompletedForTesting =
      base::RepeatingCallback<void(base::WeakPtr<PrefetchContainer>)>;
  void SetOnPrefetchResponseCompletedForTesting(
      OnPrefetchResponseCompletedForTesting callback) {
    on_prefetch_response_completed_for_testing_ = std::move(callback);
  }

  base::WeakPtr<PrefetchContainer> MatchUrl(
      const PrefetchContainer::Key& key) const;
  std::vector<std::pair<GURL, base::WeakPtr<PrefetchContainer>>>
  GetAllForUrlWithoutRefAndQueryForTesting(
      const PrefetchContainer::Key& key) const;

  base::WeakPtr<PrefetchService> GetWeakPtr();

 private:
  // Checks whether the given |prefetch_container| is eligible for prefetch.
  // Once the eligibility is determined then |result_callback| will be called
  // with result (`PreloadingEligibility::kEligible` when eligible).
  using OnEligibilityResultCallback =
      base::OnceCallback<void(base::WeakPtr<PrefetchContainer>,
                              PreloadingEligibility eligibility)>;
  void CheckEligibilityOfPrefetch(
      const GURL& url,
      base::WeakPtr<PrefetchContainer> prefetch_container,
      OnEligibilityResultCallback result_callback) const;

  void CheckHasServiceWorker(
      const GURL& url,
      base::WeakPtr<PrefetchContainer> prefetch_container,
      OnEligibilityResultCallback result_callback) const;

  void OnGotServiceWorkerResult(
      const GURL& url,
      base::WeakPtr<PrefetchContainer> prefetch_container,
      base::Time check_has_service_worker_start_time,
      OnEligibilityResultCallback result_callback,
      ServiceWorkerCapability service_worker_capability) const;

  // Called after getting the existing cookies associated with
  // |prefetch_container|. If there are any cookies, then the prefetch is not
  // eligible.
  void OnGotCookiesForEligibilityCheck(
      const GURL& url,
      base::WeakPtr<PrefetchContainer> prefetch_container,
      OnEligibilityResultCallback result_callback,
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies) const;

  // Starts the check for whether or not there is a proxy configured for the URL
  // of |prefetch_container|. If there is an existing proxy, then the prefetch
  // is not eligible.
  void StartProxyLookupCheck(
      const GURL& url,
      base::WeakPtr<PrefetchContainer> prefetch_container,
      OnEligibilityResultCallback result_callback) const;

  // Called after looking up the proxy configuration for the URL of
  // |prefetch_container|. If there is an existing proxy, then the prefetch is
  // not eligible.
  void OnGotProxyLookupResult(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      OnEligibilityResultCallback result_callback,
      bool has_proxy) const;

  // Called once the eligibility of |prefetch_container| is determined. If the
  // prefetch is eligible it is added to the queue to be prefetched. If it is
  // not eligible, then we consider making it a decoy request.
  void OnGotEligibilityResult(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      PreloadingEligibility eligibility);

  // Called once the eligibility of a redirect for a |prefetch_container| is
  // determined. If its eligible, then the prefetch will continue, otherwise it
  // is stopped.
  void OnGotEligibilityResultForRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr redirect_head,
      base::WeakPtr<PrefetchContainer> prefetch_container,
      PreloadingEligibility eligibility);

  // Starts the network requests for as many prefetches in |prefetch_queue_| as
  // possible.
  void Prefetch();

  // Pops the first valid prefetch (determined by PrefetchDocumentManager) from
  // |prefetch_queue_|. Returns a tuple containing the popped prefetch and
  // (optionally) an already completed prefetch that needs to be evicted to make
  // space for the new prefetch. If there are no valid prefetches in the queue,
  // then (nullptr, nullptr) is returned.
  std::tuple<base::WeakPtr<PrefetchContainer>, base::WeakPtr<PrefetchContainer>>
  PopNextPrefetchContainer();

  // After |PrefetchContainerLifetimeInPrefetchService| amount of time, the
  // prefetch is deleted. Note that if
  // |PrefetchContainerLifetimeInPrefetchService| is 0 or less, then it is kept
  // forever.
  void OnPrefetchTimeout(base::WeakPtr<PrefetchContainer> prefetch);

  // Starts the given |prefetch_container|. If |prefetch_to_evict| is specified,
  // it is evicted immediately before starting |prefetch_container|.
  void StartSinglePrefetch(base::WeakPtr<PrefetchContainer> prefetch_container,
                           base::WeakPtr<PrefetchContainer> prefetch_to_evict);

  // Creates a new URL loader and starts a network request for
  // |prefetch_container|. |MakePrefetchRequest| must have been previously
  // called.
  void SendPrefetchRequest(base::WeakPtr<PrefetchContainer> prefetch_container);

  // Gets the URL loader for the given |prefetch_container|. If an override was
  // set by |SetURLLoaderFactoryForTesting|, then that will be returned instead.
  network::mojom::URLLoaderFactory* GetURLLoaderFactoryForCurrentPrefetch(
      base::WeakPtr<PrefetchContainer> prefetch_container);

  // Called when the request for |prefetch_container| is redirected.
  void OnPrefetchRedirect(base::WeakPtr<PrefetchContainer> prefetch_container,
                          const net::RedirectInfo& redirect_info,
                          network::mojom::URLResponseHeadPtr redirect_head);

  // Called when the response for |prefetch_container| has started. Based on
  // |head|, returns a status to inform the |PrefetchStreamingURLLoader| whether
  // the prefetch is servable. If servable, then `absl::nullopt` will be
  // returned, otherwise a failure status is returned.
  absl::optional<PrefetchErrorOnResponseReceived> OnPrefetchResponseStarted(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      network::mojom::URLResponseHead* head);

  // Called when the response for |prefetch_container| has completed when using
  // the streaming URL loader. Only used if |PrefetchUseStreamingURLLoader| is
  // true.
  void OnPrefetchResponseCompleted(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      const network::URLLoaderCompletionStatus& completion_status);

  // Called when the cookies from |prefetch_conatiner| are read from the
  // isolated network context and are ready to be written to the default network
  // context.
  void OnGotIsolatedCookiesForCopy(
      PrefetchContainer::Reader reader,
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies);

  enum class HandlePrefetchContainerResult {
    // No prefetch was available to be used.
    kNotAvailable,
    // There was a prefetch available but it is not usable.
    kNotUsable,
    // The prefetch will be served.
    kToBeServed,
    // The prefetch cannot be served because Cookies have changed.
    kNotToBeServedCookiesChanged,
    // The prefetch's head has not yet been received.
    kWaitForHead
  };

  using FallbackToRegularNavigationWhenPrefetchNotUsable = base::StrongAlias<
      class FallbackToRegularNavigationWhenPrefetchNotUsableTag,
      bool>;
  // Helper function for |GetPrefetchToServe| to return |prefetch_container| via
  // |on_prefetch_to_serve_ready| callback in |prefetch_match_resolver|. Starts
  // the cookie copy process for the given prefetch if needed, and updates its
  // state.
  HandlePrefetchContainerResult ReturnPrefetchToServe(
      const GURL& prefetch_url,
      PrefetchContainer::Reader reader,
      PrefetchMatchResolver& prefetch_match_resolver,
      FallbackToRegularNavigationWhenPrefetchNotUsable
          when_prefetch_not_used_fallback_to_regular_navigation =
              FallbackToRegularNavigationWhenPrefetchNotUsable(true));

  // Helper function for |GetPrefetchToServe| to wait for head of a
  // potentially matching CL in order to decide if we can use it or not for
  // the current navigation.
  // Once we make the decision to use a prefetch, call |PrepareToServe| and
  // |GetPrefetchToServe| again in order to enforce that prefetches that are
  // served are served from |prefetches_ready_to_serve_|.
  void WaitOnPrefetchToServeHead(
      const PrefetchContainer::Key& key,
      base::WeakPtr<PrefetchMatchResolver> prefetch_match_resolver,
      const GURL& prefetch_url,
      base::WeakPtr<PrefetchContainer> prefetch_container);

  // Helper function for |GetPrefetchToServe| which identifies the
  // |prefetch_container|'s that could potentially be served.
  std::vector<PrefetchContainer*> FindPrefetchContainerToServe(
      const PrefetchContainer::Key& key,
      base::WeakPtr<PrefetchServingPageMetricsContainer>
          serving_page_metrics_container);

  // Helper function for |GetPrefetchToServe| which handles a
  // |prefetch_container| that could potentially be served to the navigation.
  HandlePrefetchContainerResult HandlePrefetchContainerToServe(
      const PrefetchContainer::Key& key,
      PrefetchContainer& prefetch_container,
      PrefetchMatchResolver& prefetch_match_resolver);

  // Checks if there is a prefetch in |owned_prefetches_| with the same URL as
  // |prefetch_container| but from a different referring RenderFrameHost.
  // Records the result to a UMA histogram.
  void RecordExistingPrefetchWithMatchingURL(
      base::WeakPtr<PrefetchContainer> prefetch_container) const;

  void DumpPrefetchesForDebug() const;

  raw_ptr<BrowserContext> browser_context_;

  // Delegate provided by embedder that controls specific behavior of |this|.
  // May be nullptr if embedder doesn't provide a delegate.
  std::unique_ptr<PrefetchServiceDelegate> delegate_;

  // The custom proxy configurator for Prefetch Proxy. Only used on prefetches
  // that require the proxy.
  std::unique_ptr<PrefetchProxyConfigurator> prefetch_proxy_configurator_;

  // The origin prober class which manages all logic for origin probing.
  std::unique_ptr<PrefetchOriginProber> origin_prober_;

  // A FIFO queue of prefetches that have been confirmed to be eligible but have
  // not started yet.
  std::vector<base::WeakPtr<PrefetchContainer>> prefetch_queue_;

  // The set of prefetches with in progress requests.
  std::set<PrefetchContainer::Key> active_prefetches_;

  // Prefetches owned by |this|. Once the network request for a prefetch is
  // started, |this| takes ownership of the prefetch so the response can be used
  // on future page loads.
  std::map<PrefetchContainer::Key, std::unique_ptr<PrefetchContainer>>
      owned_prefetches_;

// Protects against Prefetch() being called recursively.
#if DCHECK_IS_ON()
  bool prefetch_reentrancy_guard_ = false;
#endif

  OnPrefetchResponseCompletedForTesting
      on_prefetch_response_completed_for_testing_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchService> weak_method_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_H_
