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

class PrefetchStreamingURLLoader;

// `PrefetchResponseReader` stores the prefetched data needed for serving, and
// serves a URLLoaderClient (`serving_url_loader_client_`). One
// `PrefetchResponseReader` corresponds to one
// `PrefetchContainer::SinglePrefetch`, i.e. one redirect hop.
//
// A sequences of events are received from `PrefetchStreamingURLLoader` and
// served to `serving_url_loader_client_`.
//
// `PrefetchResponseReader` is kept alive by:
// - `PrefetchContainer::SinglePrefetch::response_reader_`
//   as long as `PrefetchContainer` is alive,
// - `PrefetchResponseReader::self_pointer_`
//   while it is serving to its `mojom::URLLoaderClient`, or
// - The `RequestHandler` returned by `CreateRequestHandler()`
//   until it is called.
class CONTENT_EXPORT PrefetchResponseReader final
    : public network::mojom::URLLoader,
      public base::RefCounted<PrefetchResponseReader> {
 public:
  PrefetchResponseReader();

  void SetStreamingURLLoader(
      base::WeakPtr<PrefetchStreamingURLLoader> streaming_url_loader);
  base::WeakPtr<PrefetchStreamingURLLoader> GetStreamingLoader() const;

  // Asynchronously release `self_pointer_` if eligible. Note that `this` might
  // be still be kept alive by others even after that.
  void MaybeReleaseSoonSelfPointer();

  // Adds events (plumbing either directly to `serving_url_loader_client_` or
  // via `AddEventToQueue()`) from the methods with the same names in
  // `PrefetchStreamingURLLoader`.
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints);
  void OnReceiveResponse(PrefetchStreamingURLLoaderStatus status,
                         network::mojom::URLResponseHeadPtr head,
                         mojo::ScopedDataPipeConsumerHandle body);
  void HandleRedirect(PrefetchRedirectStatus redirect_status,
                      const net::RedirectInfo& redirect_info,
                      network::mojom::URLResponseHeadPtr redirect_head);
  void OnTransferSizeUpdated(int32_t transfer_size_diff);
  void OnComplete(network::URLLoaderCompletionStatus completion_status);

  using RequestHandler = base::OnceCallback<void(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client)>;

  // Creates a request handler to serve the response of the prefetch.
  RequestHandler CreateRequestHandler();

  bool Servable(base::TimeDelta cacheable_duration) const;
  bool IsWaitingForResponse() const;
  absl::optional<network::URLLoaderCompletionStatus> GetCompletionStatus()
      const {
    return completion_status_;
  }
  const network::mojom::URLResponseHead* GetHead() const { return head_.get(); }

  base::WeakPtr<PrefetchResponseReader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class base::RefCounted<PrefetchResponseReader>;

  ~PrefetchResponseReader() override;

  void BindAndStart(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Adds an event to the queue that will be run when serving the prefetch.
  void AddEventToQueue(base::OnceClosure closure);

  // Sends all stored events in |event_queue_| to |serving_url_loader_client_|.
  void RunEventQueue();

  // Helper functions to send the appropriate events to
  // |serving_url_loader_client_|.
  void ForwardCompletionStatus();
  void ForwardEarlyHints(network::mojom::EarlyHintsPtr early_hints);
  void ForwardTransferSizeUpdate(int32_t transfer_size_diff);
  void ForwardRedirect(const net::RedirectInfo& redirect_info,
                       network::mojom::URLResponseHeadPtr);
  void ForwardResponse(mojo::ScopedDataPipeConsumerHandle body);

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

  void OnServingURLLoaderMojoDisconnect();

  PrefetchStreamingURLLoaderStatus GetStatusForRecording() const;

  // The URL Loader events that occur before serving the prefetch are queued up
  // until the prefetch is served.
  std::vector<base::OnceClosure> event_queue_;

  // The status of the event queue.
  enum class EventQueueStatus {
    kNotStarted,
    kRunning,
    kFinished,
  };
  EventQueueStatus event_queue_status_{EventQueueStatus::kNotStarted};

  // Valid state transitions (which imply valid event sequences) are:
  // - Redirect: `kStarted` -> `kRedirectHandled`
  // - Non-redirect: `kStarted` -> `kResponseReceived` -> `kCompleted`
  // - Failure: `kStarted` -> `kFailed`
  //            `kStarted` -> `kFailedResponseReceived` -> `kFailed`
  //            `kStarted` -> `kResponseReceived` -> `kFailed`
  // Optional `OnReceiveEarlyHints()` and `OnTransferSizeUpdated()` events can
  // be received in any non-final states.
  enum class LoadState {
    // Initial state, not yet receiving a redirect nor non-redirect response.
    kStarted,

    // [Final] A redirect response is received (`HandleRedirect()` is called).
    // This is a final state because we always switch to a new
    // `PrefetchResponseReader` on redirects.
    kRedirectHandled,

    // [servable] A non-redirect successful response is received
    // (`OnReceiveResponse()` is called with `servable` = true).
    kResponseReceived,

    // A non-redirect failed response is received (`OnReceiveResponse()` is
    // called with `servable` = false).
    kFailedResponseReceived,

    // [Final, servable] Successful completion (`OnComplete(net::OK)` is called
    // after `kResponseReceived`.
    kCompleted,

    // [Final] Failed completion (`OnComplete()` is called, either with
    // non-`net::OK`, or after `kFailedResponseReceived`).
    kFailed
  };

  LoadState load_state_{LoadState::kStarted};

  // Used for UMA recording.
  absl::optional<PrefetchStreamingURLLoaderStatus> failure_reason_;
  bool served_before_completion_{false};
  bool served_after_completion_{false};
  bool should_record_metrics_{true};

  // The prefetched data and metadata. Not set for a redirect response.
  network::mojom::URLResponseHeadPtr head_;
  absl::optional<network::URLLoaderCompletionStatus> completion_status_;
  absl::optional<base::TimeTicks> response_complete_time_;

  // The URL loader client that will serve the prefetched data.
  mojo::Receiver<network::mojom::URLLoader> serving_url_loader_receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> serving_url_loader_client_;

  // Set when this manages its own lifetime.
  scoped_refptr<PrefetchResponseReader> self_pointer_;

  base::WeakPtr<PrefetchStreamingURLLoader> streaming_url_loader_;

  base::WeakPtrFactory<PrefetchResponseReader> weak_ptr_factory_{this};
};

// Lifetime and ownership:
//
// Before `PrefetchContainer::CreateRequestHandler()`,
// `PrefetchStreamingURLLoader` is owned by `PrefetchContainer`. After that, it
// is self-owned and is deleted when `prefetch_url_loader_` is finished. The
// PrefetchStreamingURLLoader can be deleted in one of its callbacks, so instead
// of deleting it immediately, it is made self owned and then deletes itself
// asynchronously.
class CONTENT_EXPORT PrefetchStreamingURLLoader
    : public network::mojom::URLLoaderClient {
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
      network::mojom::URLResponseHeadPtr response_head)>;

  static std::unique_ptr<PrefetchStreamingURLLoader> Create(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const network::ResourceRequest& request,
      const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
      base::TimeDelta timeout_duration,
      OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
      OnPrefetchResponseCompletedCallback
          on_prefetch_response_completed_callback,
      OnPrefetchRedirectCallback on_prefetch_redirect_callback,
      base::OnceClosure on_received_head_callback,
      base::WeakPtr<PrefetchResponseReader> response_reader);

  // Must be called only from `Create()`.
  PrefetchStreamingURLLoader(
      OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
      OnPrefetchResponseCompletedCallback
          on_prefetch_response_completed_callback,
      OnPrefetchRedirectCallback on_prefetch_redirect_callback,
      base::OnceClosure on_received_head_callback);

  ~PrefetchStreamingURLLoader() override;

  PrefetchStreamingURLLoader(const PrefetchStreamingURLLoader&) = delete;
  PrefetchStreamingURLLoader& operator=(const PrefetchStreamingURLLoader&) =
      delete;

  void SetResponseReader(base::WeakPtr<PrefetchResponseReader> response_reader);

  // Informs the URL loader of how to handle the most recent redirect. This
  // should only be called after |on_prefetch_redirect_callback_| is called. The
  // value of |new_status| should only be one of the following:
  // - |kFollowRedirect|, if the redirect should be followed by |this|.
  // - |kStopSwitchInNetworkContextForRedirect|, if the redirect will be
  //   followed by a different |PrefetchStreamingURLLoader| due to a change in
  //   network context.
  // - |kFailedInvalidRedirect|, if the redirect should not be followed by
  //   |this|.
  void HandleRedirect(PrefetchRedirectStatus redirect_status,
                      const net::RedirectInfo& redirect_info,
                      network::mojom::URLResponseHeadPtr redirect_head);

  void MakeSelfOwned(std::unique_ptr<PrefetchStreamingURLLoader> self);
  void PostTaskToDeleteSelf();
  void PostTaskToDeleteSelfIfDisconnected();

  // Called from PrefetchResponseReader.
  void SetPriority(net::RequestPriority priority, int32_t intra_priority_value);
  void PauseReadingBodyFromNet();
  void ResumeReadingBodyFromNet();

  base::WeakPtr<PrefetchStreamingURLLoader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnStartServing();

 private:
  void Start(network::mojom::URLLoaderFactory* url_loader_factory,
             const network::ResourceRequest& request,
             const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
             base::TimeDelta timeout_duration);

  void DisconnectPrefetchURLLoaderMojo();

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

  // Set when this manages its own lifetime.
  std::unique_ptr<PrefetchStreamingURLLoader> self_pointer_;

  // The timer that triggers a timeout when a request takes too long.
  base::OneShotTimer timeout_timer_;

  // Once prefetching is complete, then this can be deleted.
  bool prefetch_url_loader_disconnected_{false};

  // The URL loader used to request the prefetch.
  mojo::Remote<network::mojom::URLLoader> prefetch_url_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient>
      prefetch_url_loader_client_receiver_{this};

  // Callbacks used to inform the caller of specific events of the prefetch
  // request.
  OnPrefetchResponseStartedCallback on_prefetch_response_started_callback_;
  OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback_;
  OnPrefetchRedirectCallback on_prefetch_redirect_callback_;

  // Called once it is determined whether or not the prefetch is servable, i.e.
  // either when non-redirect response head is received, or when determined not
  // servable.
  base::OnceClosure on_received_head_callback_;

  base::WeakPtr<PrefetchResponseReader> response_reader_;

  base::WeakPtrFactory<PrefetchStreamingURLLoader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_H_
