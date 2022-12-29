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
  // This callback is used fo the owner to determine if the prefetch is valid
  // based on |head|. If the prefetch should be servable based on |head|, then
  // the callback should return |kHeadReceivedWaitingOnBody|. Otherwise it
  // should return a valid failure reason.
  using OnPrefetchResponseStartedCallback =
      base::OnceCallback<PrefetchStreamingURLLoaderStatus(
          network::mojom::URLResponseHead* head)>;

  using OnPrefetchResponseCompletedCallback = base::OnceCallback<void(
      const network::URLLoaderCompletionStatus& completion_status)>;
  using OnPrefetchRedirectCallback = base::RepeatingCallback<void(
      const net::RedirectInfo& redirect_info,
      const network::mojom::URLResponseHead& response_head,
      std::vector<std::string>* removed_headers)>;

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

  // Registers a callback that is called once the head of the response is
  // received via either |OnReceiveResponse| or |OnReceiveRedirect|. The
  // callback is called once it is determined whether or not the prefetch is
  // servable.
  void SetOnReceivedHeadCallback(base::OnceClosure on_received_head_callback);

  bool Servable(base::TimeDelta cacheable_duration) const;

  absl::optional<network::URLLoaderCompletionStatus> GetCompletionStatus()
      const {
    return completion_status_;
  }
  const network::mojom::URLResponseHead* GetHead() const { return head_.get(); }

  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client)>;
  RequestHandler ServingResponseHandler(
      std::unique_ptr<PrefetchStreamingURLLoader> self);

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

  // Sends all stored events in |event_queue_| to |serving_url_loader_client_|.
  void RunEventQueue();

  // Sends the |completion_status_| to |serving_url_loader_client_|.
  void ForwardCompletionStatus();

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

  // The URL Loader events that occur before serving the prefetch are queued up
  // until the prefetch is served.
  std::vector<base::OnceClosure> event_queue_;

  // The URL loader client that will serve the prefetched data.
  mojo::Receiver<network::mojom::URLLoader> serving_url_loader_receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> serving_url_loader_client_;

  base::WeakPtrFactory<PrefetchStreamingURLLoader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_H_
