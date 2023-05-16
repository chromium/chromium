// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_status.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace content {

class CONTENT_EXPORT PrefetchStreamingURLLoader
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient {
 public:
  // This callback is used by the owner to determine if the prefetch is valid
  // based on |head|. If the prefetch should be servable based on |head|, then
  // the callback should return |kHeadReceivedWaitingOnBody|. Otherwise it
  // should return a valid failure reason.
  using OnPrefetchResponseStartedCallback =
      base::OnceCallback<PrefetchStreamingURLLoaderStatus(
          network::mojom::URLResponseHead* head)>;

  using OnPrefetchResponseCompletedCallback = base::OnceCallback<void(
      const network::URLLoaderCompletionStatus& completion_status)>;

  // This callback is used by the owner to determine if the redirect should be
  // followed. |HandleRedirect| should be called with the appropriate status for
  // how the redirect should be handled.
  using OnPrefetchRedirectCallback = base::RepeatingCallback<void(
      const net::RedirectInfo& redirect_info,
      const network::mojom::URLResponseHead& response_head)>;

  PrefetchStreamingURLLoader(
      network::mojom::URLLoaderFactory* url_loader_factory,
      std::unique_ptr<network::ResourceRequest> request,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      base::TimeDelta timeout_duration,
      OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
      OnPrefetchResponseCompletedCallback
          on_prefetch_response_completed_callback,
      OnPrefetchRedirectCallback on_prefetch_redirect_callback);
  ~PrefetchStreamingURLLoader() override;

  PrefetchStreamingURLLoader(const PrefetchStreamingURLLoader&) = delete;
  PrefetchStreamingURLLoader& operator=(const PrefetchStreamingURLLoader&) =
      delete;

  // Informs the URL loader of how to handle the most recent redirect. This
  // should only be called after |on_prefetch_redirect_callback_| is called. The
  // value of |new_status| should only be one of the following:
  // - |kFollowRedirect|, if the redirect should be followed by |this|.
  // - |kStopSwitchInNetworkContextForRedirect|, if the redirect will be
  //   followed by a different |PrefetchStreamingURLLoader| due to a change in
  //   network context.
  // - |kFailedInvalidRedirect|, if the redirect should not be followed by
  //   |this|.
  void HandleRedirect(PrefetchStreamingURLLoaderStatus new_status);

  // Registers a callback that is called once the head of the response is
  // received via either |OnReceiveResponse| or |OnReceiveRedirect|. The
  // callback is called once it is determined whether or not the prefetch is
  // servable.
  void SetOnReceivedHeadCallback(base::OnceClosure on_received_head_callback);

  // Releases the closure registered via |SetOnReceivedHeadCallback|.
  base::OnceClosure ReleaseOnReceivedHeadCallback();

  bool Servable(base::TimeDelta cacheable_duration) const;
  bool Failed() const;

  absl::optional<network::URLLoaderCompletionStatus> GetCompletionStatus()
      const {
    return completion_status_;
  }
  const network::mojom::URLResponseHead* GetHead() const { return head_.get(); }

  // Whether |this| is ready to serve its last set of events. This can either be
  // the head and body of the overall prefetch, or a redirect that required a
  // switch in network context and was followed in a separate
  // |PrefetchStreamingURLLoader|.
  bool IsReadyToServeLastEvents() const;

  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client)>;

  // Creates a request handler to serve the final response of the prefetch, and
  // also makes |this| self owned.
  RequestHandler ServingFinalResponseHandler(
      std::unique_ptr<PrefetchStreamingURLLoader> self);

  // Creates a request handler to serve the next redirect. Ownership of |this|
  // does not change.
  RequestHandler ServingRedirectHandler();

  // The streaming URL loader can be deleted in one of its callbacks, so instead
  // of deleting it immediately, it is made self owned and then deletes itself.
  void MakeSelfOwnedAndDeleteSoon(
      std::unique_ptr<PrefetchStreamingURLLoader> self);

  base::WeakPtr<PrefetchStreamingURLLoader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void BindAndStart(
      std::unique_ptr<PrefetchStreamingURLLoader> self,
      const network::ResourceRequest& request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client);

  // Adds an event to the queue that will be run when serving the prefetch. If
  // |pause_after_event| is true, then the event queue will pause after running
  // the event.
  void AddEventToQueue(base::OnceClosure closure, bool pause_after_event);

  // Sends all stored events in |event_queue_| to |serving_url_loader_client_|.
  void RunEventQueue();

  // Helper functions to send the appropriate events to
  // |serving_url_loader_client_|.
  void ForwardCompletionStatus();
  void ForwardEarlyHints(network::mojom::EarlyHintsPtr early_hints);
  void ForwardTransferSizeUpdate(int32_t transfer_size_diff);
  void ForwardRedirect(const net::RedirectInfo& redirect_info,
                       network::mojom::URLResponseHeadPtr);
  void ForwardResponse();

  void DisconnectPrefetchURLLoaderMojo();
  void OnServingURLLoaderMojoDisconnect();
  void PostTaskToDeleteSelf();

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      absl::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(
      const network::URLLoaderCompletionStatus& completion_status) override;

  // network::mojom::URLLoader
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  // Set when this manages its own lifetime.
  std::unique_ptr<PrefetchStreamingURLLoader> self_pointer_;

  // Status of the URL loader. This recorded to UMA when the URL loader is
  // deleted.
  PrefetchStreamingURLLoaderStatus status_{
      PrefetchStreamingURLLoaderStatus::kWaitingOnHead};

  // The timer that triggers a timeout when a request takes too long.
  base::OneShotTimer timeout_timer_;

  // Once prefetching and serving is complete, then this can be deleted.
  bool prefetch_url_loader_disconnected_{false};
  bool serving_url_loader_disconnected_{false};

  // The URL loader used to request the prefetch.
  mojo::Remote<network::mojom::URLLoader> prefetch_url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient>
      prefetch_url_loader_client_receiver_{this};

  // Callbacks used to inform the caller of specific events of the prefetch
  // request.
  OnPrefetchResponseStartedCallback on_prefetch_response_started_callback_;
  OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback_;
  OnPrefetchRedirectCallback on_prefetch_redirect_callback_;
  base::OnceClosure on_received_head_callback_;

  // The prefetched data and metadata.
  network::mojom::URLResponseHeadPtr head_;
  mojo::ScopedDataPipeConsumerHandle body_;
  bool servable_{false};
  absl::optional<network::URLLoaderCompletionStatus> completion_status_;
  absl::optional<base::TimeTicks> response_complete_time_;

  // These store the most recent redirect in the event that |this| needs to wait
  // for the prefetch eligibility check to complete before deciding whether to
  // follow the redirect or not.
  net::RedirectInfo redirect_info_;
  network::mojom::URLResponseHeadPtr redirect_head_;

  // The URL Loader events that occur before serving the prefetch are queued up
  // until the prefetch is served. The first value is the closure to run the
  // event, and the second value is whether or not the event queue should be
  // paused after running the event.
  std::vector<std::pair<base::OnceClosure, bool>> event_queue_;

  // The status of the event queue.
  enum class EventQueueStatus {
    kNotStarted,
    kRunning,
    kPaused,
    kFinished,
  };
  EventQueueStatus event_queue_status_{EventQueueStatus::kNotStarted};

  // The URL loader client that will serve the prefetched data.
  mojo::Receiver<network::mojom::URLLoader> serving_url_loader_receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> serving_url_loader_client_;

  base::WeakPtrFactory<PrefetchStreamingURLLoader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_H_
