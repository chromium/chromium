// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_H_

#include <map>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "net/cookies/canonical_cookie.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace base {
class OneShotTimer;
}  // namespace base

namespace network::mojom {
class URLLoaderFactory;
}  // namespace network::mojom

namespace net {
class IsolationInfo;
}  // namespace net

namespace content {

class BrowserContext;
class PrefetchOriginProber;
class PrefetchProxyConfigurator;
class PrefetchServiceDelegate;
class ServiceWorkerContext;

// Manages all prefetches within a single BrowserContext. Responsible for
// checking the eligibility of the prefetch, making the network request for the
// prefetch, and provide prefetched resources to URL loader interceptor when
// needed.
class CONTENT_EXPORT PrefetchService {
 public:
  // |browser_context| must outlive this instance. In general this should always
  // be true, since |PrefetchService| will be indirectly owned by
  // |BrowserContext|.
  static std::unique_ptr<PrefetchService> CreateIfPossible(
      BrowserContext* browser_context);

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

  PrefetchOriginProber* GetPrefetchOriginProber() const {
    return origin_prober_.get();
  }

  virtual void PrefetchUrl(base::WeakPtr<PrefetchContainer> prefetch_container);

  // Called when a navigation to the URL associated with |prefetch_container| is
  // likely to occur in the immediate future.
  void PrepareToServe(base::WeakPtr<PrefetchContainer> prefetch_container);

  // Returns the prefetch with |url| that is ready to serve. In order for a
  // prefetch to be ready to serve, |PrepareToServe| must have been previously
  // called with the prefetch.
  base::WeakPtr<PrefetchContainer> GetPrefetchToServe(const GURL& url) const;

  // Returns the current prefetches associated with |this|. Used to check the
  // state of the prefetches.
  // TODO(https://crbug.com/1299059): Remove this once we can get metrics
  // instead.
  const std::map<PrefetchContainer::Key, base::WeakPtr<PrefetchContainer>>&
  GetAllPrefetchesForTesting() {
    return all_prefetches_;
  }

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

 private:
  // Checks whether the given |prefetch_container| is eligible for prefetch.
  // Once the eligibility is determined then |result_callback| will be called
  // with result and an optional status stating why the prefetch is not
  // eligible.
  using OnEligibilityResultCallback =
      base::OnceCallback<void(base::WeakPtr<PrefetchContainer>,
                              bool eligible,
                              absl::optional<PrefetchStatus> status)>;
  void CheckEligibilityOfPrefetch(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      OnEligibilityResultCallback result_callback) const;

  // Called after getting the existing cookies associated with
  // |prefetch_container|. If there are any cookies, then the prefetch is not
  // eligible.
  void OnGotCookiesForEligibilityCheck(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      OnEligibilityResultCallback result_callback,
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies) const;

  // Called once the eligibility of |prefetch_container| is determined. If the
  // prefetch is eligible it is added to the queue to be prefetched. If it is
  // not eligible, then we consider making it a decoy request.
  void OnGotEligibilityResult(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      bool eligible,
      absl::optional<PrefetchStatus> status);

  // Starts the network requests for as many prefetches in |prefetch_queue_| as
  // possible.
  void Prefetch();

  // Pops the first valid prefetch from |prefetch_queue_|. If there are no
  // valid prefetches in the queue, then nullptr is returned. In this context,
  // for a prefetch to be valid, it must not be null and it must be on a visible
  // web contents.
  base::WeakPtr<PrefetchContainer> PopNextPrefetchContainer();

  // Once the network request for a prefetch starts, ownership is transferred
  // from the referring |PrefetchDocumentManager| to |this|. After
  // |PrefetchContainerLifetimeInPrefetchService| amount of time, the prefetch
  // is deleted. Note that if |PrefetchContainerLifetimeInPrefetchService| is 0
  // or less, then it is kept forever.
  void TakeOwnershipOfPrefetch(
      base::WeakPtr<PrefetchContainer> prefetch_container);
  void ResetPrefetch(base::WeakPtr<PrefetchContainer> prefetch_container);

  // Starts the network request for the given |prefetch_container|.
  void StartSinglePrefetch(base::WeakPtr<PrefetchContainer> prefetch_container);

  // Gets the URL loader for the given |prefetch_container|. If an override was
  // set by |SetURLLoaderFactoryForTesting|, then that will be returned instead.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory(
      base::WeakPtr<PrefetchContainer> prefetch_container);

  // Called when the request for |prefetch_container| is redirected.
  void OnPrefetchRedirect(base::WeakPtr<PrefetchContainer> prefetch_container,
                          const net::RedirectInfo& redirect_info,
                          const network::mojom::URLResponseHead& response_head,
                          std::vector<std::string>* removed_headers);

  // Called when the request for |prefetch_container| is completed.
  void OnPrefetchComplete(base::WeakPtr<PrefetchContainer> prefetch_container,
                          const net::IsolationInfo& isolation_info,
                          std::unique_ptr<std::string> body);

  // Checks the response form |OnPrefechComplete| for success or failure. On
  // success, the response is moved to a |PrefetchedMainframeResponseContainer|
  // and cached in |prefetch_contianer|.
  void HandlePrefetchedResponse(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      const net::IsolationInfo& isolation_info,
      network::mojom::URLResponseHeadPtr head,
      std::unique_ptr<std::string> body);

  // Copies any cookies in the isolated network context associated with
  // |prefetch_container| to the default network context.
  void CopyIsolatedCookies(base::WeakPtr<PrefetchContainer> prefetch_container);
  void OnGotIsolatedCookiesForCopy(
      base::WeakPtr<PrefetchContainer> prefetch_container,
      const net::CookieAccessResultList& cookie_list,
      const net::CookieAccessResultList& excluded_cookies);

  // Checks if there is a prefetch in |all_prefetches_| with the same URL as
  // |prefetch_container| but from a different referring render frame host.
  // Records the result to a UMA histogram.
  void RecordExistingPrefetchWithMatchingURL(
      base::WeakPtr<PrefetchContainer> prefetch_container) const;

  raw_ptr<BrowserContext> browser_context_;

  // Delegate provided by embedder that controls specific behavior of |this|.
  // May be nullptr if embedder doesn't provide a delegate.
  std::unique_ptr<PrefetchServiceDelegate> delegate_;

  // The custom proxy configurator for Prefetch Proxy. Only used on prefetches
  // that require the proxy.
  std::unique_ptr<PrefetchProxyConfigurator> prefetch_proxy_configurator_;

  // The origin prober class which manages all logic for origin probing.
  std::unique_ptr<PrefetchOriginProber> origin_prober_;

  // All prefetches associated with |this| regardless of ownership.
  std::map<PrefetchContainer::Key, base::WeakPtr<PrefetchContainer>>
      all_prefetches_;

  // A FIFO queue of prefetches that have been confirmed to be eligible but have
  // not started yet.
  std::vector<base::WeakPtr<PrefetchContainer>> prefetch_queue_;

  // The number of prefetches with in progress requests.
  int num_active_prefetches_{0};

  // Prefetches owned by |this|. Once the network request for a prefetch is
  // started, |this| takes ownership of the prefetch so the response can be used
  // on future page loads. A timer of
  // |PrefetchContainerLifetimeInPrefetchService| is set that deletes the
  // prefetch. If |PrefetchContainerLifetimeInPrefetchService| zero or less,
  // then, the prefetch is kept forever.
  std::map<PrefetchContainer::Key,
           std::pair<std::unique_ptr<PrefetchContainer>,
                     std::unique_ptr<base::OneShotTimer>>>
      owned_prefetches_;

  // The set of prefetches that are ready to serve. In order to be in this map,
  // the prefetch must also be in |owned_prefetches_|, have a valid prefetched
  // response, and have started the cookie copy process. A prefetch is added to
  // this map when |PrepareToServe| is called on it, and once in this map, it
  // can be returned by |GetPrefetchToServe|.
  std::map<GURL, base::WeakPtr<PrefetchContainer>> prefetches_ready_to_serve_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<PrefetchService> weak_method_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_SERVICE_H_
