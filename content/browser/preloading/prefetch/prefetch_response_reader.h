// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESPONSE_READER_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESPONSE_READER_H_

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader_common_types.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/http/http_cookie_indices.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

namespace ukm::builders {
class PrefetchProxy_PrefetchedResource;
}  // namespace ukm::builders

namespace content {

class PrefetchContainer;
class PrefetchDataPipeTee;
class PrefetchStreamingURLLoader;
class ServiceWorkerClient;
class ServiceWorkerMainResourceHandle;

// `PrefetchResponseReader` stores the prefetched data needed for serving, and
// serves URLLoaderClients (`serving_url_loader_clients_`). One
// `PrefetchResponseReader` corresponds to one
// `PrefetchContainer::SinglePrefetch`, i.e. one redirect hop.
//
// A sequences of events are received from `PrefetchStreamingURLLoader` and
// served to each of `serving_url_loader_clients_`.
//
// `PrefetchResponseReader` is kept alive by:
// - `PrefetchContainer::SinglePrefetch::response_reader_`
//   as long as `PrefetchContainer` is alive,
// - `PrefetchResponseReader::self_pointer_`
//   while it is serving to its `mojom::URLLoaderClient`, or
// - The `PrefetchRequestHandler` returned by `CreateRequestHandler()`
//   until it is called.
//
// TODO(crbug.com/40064891): Currently at most one client per
// `PrefetchResponseReader` is allowed due to other servablility conditions.
// Upcoming CLs will enable multiple clients/navigation requests per
// `PrefetchResponseReader` behind a flag.
class CONTENT_EXPORT PrefetchResponseReader final
    : public network::mojom::URLLoader,
      public base::RefCounted<PrefetchResponseReader> {
 public:
  PrefetchResponseReader(
      OnPrefetchDeterminedHeadCallback on_determined_head_callback,
      OnPrefetchResponseCompletedCallback
          on_prefetch_response_completed_callback);

  void SetStreamingURLLoader(
      base::WeakPtr<PrefetchStreamingURLLoader> streaming_url_loader);
  base::WeakPtr<PrefetchStreamingURLLoader> GetStreamingLoader() const;

  // Asynchronously release `self_pointer_` if eligible. Note that `this` might
  // be still be kept alive by others even after that.
  void MaybeReleaseSoonSelfPointer();

  // Adds events from the methods with the same names in
  // `PrefetchStreamingURLLoader` to `event_queue_` and existing
  // `serving_url_loader_clients_`.
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints);
  void OnReceiveResponse(
      std::optional<PrefetchErrorOnResponseReceived> status,
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle);
  void HandleRedirect(PrefetchRedirectStatus redirect_status,
                      const net::RedirectInfo& redirect_info,
                      network::mojom::URLResponseHeadPtr redirect_head);
  void OnTransferSizeUpdated(int32_t transfer_size_diff);
  void OnComplete(network::URLLoaderCompletionStatus completion_status);

  // Creates a request handler to serve the response of the prefetch.
  //
  // `CreateRequestHandler()` is responsible for the final check for servability
  // and can return a null PrefetchRequestHandler if the final check fails (even
  // if `GetServableState()` previously returned `kServable`).
  //
  // The caller is responsible for:
  // - Cookie-related checks and processing.
  //   For example, checking `HaveDefaultContextCookiesChanged()` is false and
  //   copying isolated cookies if needed.
  //   `PrefetchResponseReader::CreateRequestHandler()`,
  //   `PrefetchResponseReader::Servable()` nor
  //   `PrefetchContainer::GetServableState()` don't perform cookie-related
  //   checks.
  // - Checking `Servable()`/`GetServableState()`.
  //   `cacheable_duration` is checked only there.
  std::pair<PrefetchRequestHandler, base::WeakPtr<ServiceWorkerClient>>
  CreateRequestHandler();

  bool Servable(base::TimeDelta cacheable_duration) const;
  bool IsWaitingForResponse() const;
  const network::mojom::URLResponseHead* GetHead() const { return head_.get(); }

  // True if this response had Vary: Cookie (or Vary: *), and a Cookie-Indices
  // header also applies.
  bool VariesOnCookieIndices() const;

  // True if the request cookies `cookies` match those originally used when the
  // prefetch request was made, to the extent required by Cookie-Indices.
  // Do not call this if |VariesOnCookieIndices()| returns false.
  bool MatchesCookieIndices(
      base::span<const std::pair<std::string, std::string>> cookies) const;

  void RecordOnPrefetchContainerDestroyed(
      base::PassKey<PrefetchContainer>,
      ukm::builders::PrefetchProxy_PrefetchedResource& builder) const;

  // Valid state transitions (which imply valid event sequences) are:
  // - Redirect: `kStarted` -> `kRedirectHandled`
  // - Non-redirect: `kStarted` -> `kResponseReceived` -> `kCompleted`
  // - Failure: `kStarted` -> `kFailed`
  //            `kStarted` -> `kFailedRedirect`
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
    kFailed,

    // [Final] Failed redirects.
    kFailedRedirect
  };

  LoadState load_state() const { return load_state_; }

  base::WeakPtr<PrefetchResponseReader> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Identifies a client in `serving_url_loader_clients_`.
  using ServingUrlLoaderClientId = mojo::RemoteSetElementId;

  friend class base::RefCounted<PrefetchResponseReader>;
  // This is necessary because `PrefetchContainerObserver` emulates a callback
  // that we will provide in the future.
  //
  // TODO(crbug.com/400761083): Remove it.
  friend class PrefetchContainerObserver;

  ~PrefetchResponseReader() override;

  void BindAndStart(
      mojo::ScopedDataPipeConsumerHandle body,
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client);

  // Adds an event to the queue.
  // The callbacks are called in-order for each of
  // `serving_url_loader_clients_`, regardless of whether events are added
  // before or after clients are added.
  using EventCallback = base::RepeatingCallback<void(ServingUrlLoaderClientId)>;
  void AddEventToQueue(EventCallback callback);
  // Sends all stored events in `event_queue_` to the client.
  // Called when a new client (identified by `client_id_`) is added.
  void RunEventQueue(ServingUrlLoaderClientId client_id);

  // Helper functions to send the appropriate events to a client.
  void ForwardCompletionStatus(ServingUrlLoaderClientId client_id);
  void ForwardEarlyHints(const network::mojom::EarlyHintsPtr& early_hints,
                         ServingUrlLoaderClientId client_id);
  void ForwardTransferSizeUpdate(int32_t transfer_size_diff,
                                 ServingUrlLoaderClientId client_id);
  void ForwardRedirect(const net::RedirectInfo& redirect_info,
                       const network::mojom::URLResponseHeadPtr&,
                       ServingUrlLoaderClientId client_id);
  void ForwardResponse(ServingUrlLoaderClientId client_id);

  // network::mojom::URLLoader
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;

  void OnServingURLLoaderMojoDisconnect();

  PrefetchStreamingURLLoaderStatus GetStatusForRecording() const;

  // Stores info from the response head that will be needed later, before it is
  // stored into `head_` (for non-redirect responses) or `event_queue_` (or
  // redirect responses).
  void StoreInfoFromResponseHead(const network::mojom::URLResponseHead& head);

  // All URLLoader events are queued up here.
  std::vector<EventCallback> event_queue_;

  // The status of the event queue.
  enum class EventQueueStatus {
    kNotRunning,
    kRunning,
  };
  EventQueueStatus event_queue_status_{EventQueueStatus::kNotRunning};

  // Always access/update through `load_state()` and
  // `SetLoadStateAndAddEventToQueue()` below, to avoid unintentional state
  // changes and missing related callbacks on state changes.
  LoadState load_state_{LoadState::kStarted};

  void SetLoadStateAndAddEventToQueue(LoadState new_load_state,
                                      EventCallback callback);

  // Called when transitioned for the first time to a state other than
  // `kStarted` nor `kRedirectHandled`.
  //
  // This should be always called once for the entire `PrefetchResponseReader`s
  // for a given `PrefetchContainer`.
  // TODO(https://crbug.com/400761083): This isn't called for:
  // - unexpected mojo disconnection cases (See
  //   `PrefetchStreamingURLLoaderTest.UnexpectedUrlLoaderDisconnect`).
  OnPrefetchDeterminedHeadCallback on_determined_head_callback_;

  // Called when transitioned to `kCompleted` or `kFailed`.
  // This is called after `on_determined_head_callback_` at most once for the
  // entire `PrefetchResponseReader`s for a given `PrefetchContainer`.
  // TODO(https://crbug.com/400761083): This isn't called for:
  // - `kFailedRedirect` (See
  //   `PrefetchStreamingURLLoaderTest.IneligibleRedirect`) or
  // - unexpected mojo disconnection cases (See
  //   `PrefetchStreamingURLLoaderTest.UnexpectedUrlLoaderDisconnect`).
  OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback_;

  // Used for UMA recording.
  // TODO(crbug.com/40064891): we might want to adapt these flags and UMA
  // semantics for multiple client settings, but so far we don't have any
  // specific plans.
  std::optional<PrefetchErrorOnResponseReceived> failure_reason_;
  bool served_before_completion_{false};
  bool served_after_completion_{false};
  bool should_record_metrics_{true};

  // If present, this includes the sorted and unique names of the cookies which
  // were specified in the Cookie-Indices header, and a hash of their values as
  // obtained from `net::HashCookieIndices`. This is not set unless the Vary
  // header also specified Cookie (or *).
  //
  // As one quirk, we presently still don't vary on cookies if Vary is specified
  // and Cookie-Indices isn't, both because that was the prior behavior and
  // because doing so requires having the precise string value of the header
  // (including whitespace).
  struct CookieIndicesInfo {
    CookieIndicesInfo();
    ~CookieIndicesInfo();

    std::vector<std::string> cookie_names;
    net::CookieIndicesHash expected_hash;
  };
  std::optional<CookieIndicesInfo> cookie_indices_;

  // The prefetched data and metadata. Not set for a redirect response.
  network::mojom::URLResponseHeadPtr head_;
  scoped_refptr<PrefetchDataPipeTee> body_tee_;
  std::optional<network::URLLoaderCompletionStatus> completion_status_;
  // Recorded on `OnComplete` and used to check if the prefetch data is still
  // fresh for use.
  std::optional<base::TimeTicks> response_complete_time_;

  // Only used temporarily to plumb the body `BindAndStart()` to
  // `ForwardResponse()`.
  mojo::ScopedDataPipeConsumerHandle forward_body_;

  // The URL loader clients that will serve the prefetched data.
  mojo::ReceiverSet<network::mojom::URLLoader> serving_url_loader_receivers_;
  mojo::RemoteSet<network::mojom::URLLoaderClient> serving_url_loader_clients_;

  // Set when this manages its own lifetime.
  scoped_refptr<PrefetchResponseReader> self_pointer_;

  base::WeakPtr<PrefetchStreamingURLLoader> streaming_url_loader_;

  // TODO(https://crbug.com/40947546): Currently redirects are not supported for
  // ServiceWorker-controlled prefetches and thus we don't care about alignment
  // between `PrefetchResponseReader`, `PrefetchStreamingURLLoader` and
  // `PrefetchContainer` in terms of `ServiceWorkerMainResourceHandle`.
  std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle_;

  base::WeakPtrFactory<PrefetchResponseReader> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_RESPONSE_READER_H_
