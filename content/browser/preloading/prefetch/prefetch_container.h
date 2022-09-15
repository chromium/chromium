// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_

#include <utility>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/speculation_host_delegate.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class CookieManager;
}  // namespace mojom
}  // namespace network

namespace content {

class PrefetchCookieListener;
class PrefetchDocumentManager;
class PrefetchNetworkContext;
class PrefetchService;
class PrefetchedMainframeResponseContainer;
class ProxyLookupClientImpl;

// This class contains the state for a request to prefetch a specific URL.
class CONTENT_EXPORT PrefetchContainer {
 public:
  PrefetchContainer(
      const GlobalRenderFrameHostId& referring_render_frame_host_id,
      const GURL& url,
      const PrefetchType& prefetch_type,
      base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager);
  ~PrefetchContainer();

  PrefetchContainer(const PrefetchContainer&) = delete;
  PrefetchContainer& operator=(const PrefetchContainer&) = delete;

  // Defines the key to uniquely identify a prefetch.
  using Key = std::pair<GlobalRenderFrameHostId, GURL>;
  Key GetPrefetchContainerKey() const {
    return std::make_pair(referring_render_frame_host_id_, url_);
  }

  // The ID of the render frame host that triggered the prefetch.
  GlobalRenderFrameHostId GetReferringRenderFrameHostId() const {
    return referring_render_frame_host_id_;
  }

  // The URL that will potentially be prefetched.
  GURL GetURL() const { return url_; }

  // The type of this prefetch. Controls how the prefetch is handled.
  const PrefetchType& GetPrefetchType() const { return prefetch_type_; }

  base::WeakPtr<PrefetchContainer> GetWeakPtr() const {
    return weak_method_factory_.GetWeakPtr();
  }

  // The status of the current prefetch. Note that |HasPrefetchStatus| will be
  // initially false until |SetPrefetchStatus| is called.
  void SetPrefetchStatus(PrefetchStatus prefetch_status) {
    prefetch_status_ = prefetch_status;
  }
  bool HasPrefetchStatus() const { return prefetch_status_.has_value(); }
  PrefetchStatus GetPrefetchStatus() const;

  // Controls ownership of the |ProxyLookupClientImpl| used during the
  // eligibility check.
  void TakeProxyLookupClient(
      std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client);
  std::unique_ptr<ProxyLookupClientImpl> ReleaseProxyLookupClient();

  // Whether or not the prefetch was determined to be eligibile
  void OnEligibilityCheckComplete(bool is_eligible);

  // Whether this prefetch is a decoy. Decoy prefetches will not store the
  // response, and not serve any prefetched resources.
  void SetIsDecoy(bool is_decoy) { is_decoy_ = is_decoy; }
  bool IsDecoy() const { return is_decoy_; }

  // After the initial eligiblity check for |url_|, a
  // |PrefetchCookieListener| listens for any changes to the cookies
  // associated with |url_|. If these cookies change, then no prefetched
  // resources will be served.
  void RegisterCookieListener(network::mojom::CookieManager* cookie_manager);
  void StopCookieListener();
  bool HaveDefaultContextCookiesChanged() const;

  // Before a prefetch can be served, any cookies added to the isolated network
  // context must be copied over to the default network context. These functions
  // are used to check and update the status of this process, as well as record
  // metrics about how long this process takes.
  bool IsIsolatedCookieCopyInProgress() const;
  void OnIsolatedCookieCopyStart();
  void OnIsolatedCookiesReadCompleteAndWriteStart();
  void OnIsolatedCookieCopyComplete();
  void SetOnCookieCopyCompleteCallback(base::OnceClosure callback);

  // The network context used to make network requests for this prefetch.
  PrefetchNetworkContext* GetOrCreateNetworkContext(
      PrefetchService* prefetch_service);
  PrefetchNetworkContext* GetNetworkContext() { return network_context_.get(); }

  // The URL loader used to make the network requests for this prefetch.
  void TakeURLLoader(std::unique_ptr<network::SimpleURLLoader> loader);
  network::SimpleURLLoader* GetLoader() { return loader_.get(); }
  void ResetURLLoader();

  // The |PrefetchDocumentManager| that requested |this|.
  PrefetchDocumentManager* GetPrefetchDocumentManager() const;

  // Called when a navigation is started that could pottentially use this
  // prefetch.
  void OnNavigationToPrefetch() { navigated_to_ = true; }

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

  // Whether or not |this| has a prefetched response.
  bool HasValidPrefetchedResponse(base::TimeDelta cacheable_duration) const;

  // |this| takes ownership of the given |prefetched_response|.
  void TakePrefetchedResponse(
      std::unique_ptr<PrefetchedMainframeResponseContainer>
          prefetched_response);

  // Releases ownership of |prefetched_response_| from |this| and gives it to
  // the caller.
  std::unique_ptr<PrefetchedMainframeResponseContainer>
  ReleasePrefetchedResponse();

  // Returns the time between the prefetch request was sent and the time the
  // response headers were received. Not set if the prefetch request hasn't been
  // sent or the response headers haven't arrived.
  absl::optional<base::TimeDelta> GetPrefetchHeaderLatency() const {
    return header_latency_;
  }

  // Returns request id to be used by DevTools
  const std::string& RequestId() const { return request_id_; }

  // Sets DevTools observer
  void SetDevToolsObserver(
      base::WeakPtr<content::SpeculationHostDevToolsObserver>
          devtools_observer) {
    devtools_observer_ = std::move(devtools_observer);
  }

  // Returns DevTool observer
  const base::WeakPtr<SpeculationHostDevToolsObserver>& GetDevToolsObserver()
      const {
    return devtools_observer_;
  }

 protected:
  friend class PrefetchContainerTest;

  // Updates metrics based on the result of the prefetch request.
  void UpdatePrefetchRequestMetrics(
      const absl::optional<network::URLLoaderCompletionStatus>&
          completion_status,
      const network::mojom::URLResponseHead* head);

 private:
  // The ID of the render frame host that triggered the prefetch.
  GlobalRenderFrameHostId referring_render_frame_host_id_;

  // The URL that will potentially be prefetched
  GURL url_;

  // The type of this prefetch. This controls some specific details about how
  // the prefetch is handled, including whether an isolated network context or
  // the default network context is used to perform the prefetch, whether or
  // not the preftch proxy is used, and whether or not subresources are
  // prefetched.
  PrefetchType prefetch_type_;

  // The |PrefetchDocumentManager| that requested |this|. Initially it owns
  // |this|, but once the network request for the prefetch is started,
  // ownernship is transferred to |PrefetchService|.
  base::WeakPtr<PrefetchDocumentManager> prefetch_document_manager_;

  // The current status, if any, of the prefetch.
  absl::optional<PrefetchStatus> prefetch_status_;

  // Looks up the proxy settings in the default network context for |url_|. If
  // there is an existing proxy for |url_| then it is not eligible.
  std::unique_ptr<ProxyLookupClientImpl> proxy_lookup_client_;

  // Whether this prefetch is a decoy or not. If the prefetch is a decoy then
  // any prefetched resources will not be served.
  bool is_decoy_ = false;

  // This tracks whether the cookies associated with |url_| have changed at some
  // point after the initial eligibility check.
  std::unique_ptr<PrefetchCookieListener> cookie_listener_;

  // The network context used to prefetch |url_|.
  std::unique_ptr<PrefetchNetworkContext> network_context_;

  // The URL loader used to prefetch |url_|.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  // The prefetched response for |url_|.
  std::unique_ptr<PrefetchedMainframeResponseContainer> prefetched_response_;

  // The time at which |prefetched_response_| was received. This is used to
  // determine whether or not |prefetched_response_| is stale.
  absl::optional<base::TimeTicks> prefetch_received_time_;

  ukm::SourceId ukm_source_id_;

  // The size of the prefetched response.
  absl::optional<int> data_length_;

  // The amount  of time it took for the prefetch to complete.
  absl::optional<base::TimeDelta> fetch_duration_;

  // The amount  of time it took for the headers to be received.
  absl::optional<base::TimeDelta> header_latency_;

  // Whether or not a navigation to this prefetch occurred.
  bool navigated_to_ = false;

  // The result of probe when checked on navigation.
  absl::optional<PrefetchProbeResult> probe_result_;

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

  // Request identifier used by DevTools
  std::string request_id_;

  // Weak pointer to DevTools observer
  base::WeakPtr<SpeculationHostDevToolsObserver> devtools_observer_;

  base::WeakPtrFactory<PrefetchContainer> weak_method_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_CONTAINER_H_
