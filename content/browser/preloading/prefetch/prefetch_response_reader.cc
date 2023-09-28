// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_response_reader.h"

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

bool PrefetchResponseReader::Servable(
    base::TimeDelta cacheable_duration) const {
  bool servable = false;
  switch (load_state_) {
    case LoadState::kResponseReceived:
    case LoadState::kCompleted:
      servable = true;
      break;

    case LoadState::kStarted:
    case LoadState::kRedirectHandled:
    case LoadState::kFailedResponseReceived:
    case LoadState::kFailed:
      servable = false;
      break;
  }

  // If the response hasn't been received yet (meaning response_complete_time_
  // is absl::nullopt), we can still serve the prefetch (depending on |head_|).
  return servable && (!response_complete_time_.has_value() ||
                      base::TimeTicks::Now() <
                          response_complete_time_.value() + cacheable_duration);
}

bool PrefetchResponseReader::IsWaitingForResponse() const {
  switch (load_state_) {
    case LoadState::kStarted:
      return true;

    case LoadState::kResponseReceived:
    case LoadState::kRedirectHandled:
    case LoadState::kCompleted:
    case LoadState::kFailedResponseReceived:
    case LoadState::kFailed:
      return false;
  }
}

PrefetchResponseReader::PrefetchResponseReader() {
  serving_url_loader_receivers_.set_disconnect_handler(base::BindRepeating(
      &PrefetchResponseReader::OnServingURLLoaderMojoDisconnect,
      weak_ptr_factory_.GetWeakPtr()));
}

PrefetchResponseReader::~PrefetchResponseReader() {
  if (should_record_metrics_) {
    base::UmaHistogramEnumeration(
        "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
        GetStatusForRecording());
  }
}

void PrefetchResponseReader::SetStreamingURLLoader(
    base::WeakPtr<PrefetchStreamingURLLoader> streaming_url_loader) {
  CHECK(!streaming_url_loader_);
  streaming_url_loader_ = std::move(streaming_url_loader);
}

base::WeakPtr<PrefetchStreamingURLLoader>
PrefetchResponseReader::GetStreamingLoader() const {
  return streaming_url_loader_;
}

void PrefetchResponseReader::MaybeReleaseSoonSelfPointer() {
  if (!self_pointer_) {
    return;
  }
  if (!serving_url_loader_receivers_.empty()) {
    return;
  }

  // To avoid UAF bugs, post a separate task to possibly delete `this`.
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(self_pointer_));
}

void PrefetchResponseReader::OnServingURLLoaderMojoDisconnect() {
  MaybeReleaseSoonSelfPointer();
}

PrefetchRequestHandler PrefetchResponseReader::CreateRequestHandler() {
  if (create_request_handler_called_) {
    // Monitor cases where CreateRequestHandler() is called multiple times, for
    // investigation of crbug.com/1483599. Anyway such cases should be handled
    // (failing gracefully) below, e.g. by checking `body_`.
    // TODO(crbug.com/1483599): Remove this.
    base::debug::DumpWithoutCrashing();
  }
  create_request_handler_called_ = true;

  // Returns a null handler if some checks fail here.
  // This is a subset of the checks in `BindAndStart()`, but not identical,
  // because `load_state_` can be transitioned between the two methods. Still
  // the CHECKs in `BindAndStart()` should pass even when `load_state_` is
  // transitioned.
  switch (load_state_) {
    case LoadState::kResponseReceived:
    case LoadState::kCompleted:
    case LoadState::kFailed:
      if (!body_) {
        // This might be because `CreateRequestHandler()` is called for the
        // second time.
        return {};
      }
      break;

    case LoadState::kRedirectHandled:
      break;

    case LoadState::kStarted:
    case LoadState::kFailedResponseReceived:
      return {};
  }

  if (streaming_url_loader_) {
    streaming_url_loader_->OnStartServing();
  }

  return base::BindOnce(&PrefetchResponseReader::BindAndStart,
                        base::WrapRefCounted(this), std::move(body_));
}

void PrefetchResponseReader::BindAndStart(
    mojo::ScopedDataPipeConsumerHandle body,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  // Currently only one client is allowed.
  // TODO(crbug.com/1449360): Actually support multiple clients.
  CHECK(serving_url_loader_clients_.empty());

  serving_url_loader_receivers_.Add(this, std::move(receiver));
  ServingUrlLoaderClientId client_id =
      serving_url_loader_clients_.Add(std::move(client));
  if (!self_pointer_) {
    self_pointer_ = base::WrapRefCounted(this);
  }

  if (load_state_ == LoadState::kCompleted) {
    served_after_completion_ = true;
  } else {
    served_before_completion_ = true;
  }

  forward_body_ = std::move(body);

  switch (load_state_) {
    case LoadState::kResponseReceived:
      // In these cases, `ForwardResponse()` is expected to be called always
      // inside `RunEventQueue()` below, because `CreateRequestHandler()` was
      // called after response headers are received. Both the head and body
      // plumbed to `ForwardResponse()` should be non-null.
      //
      // The `kFailed` cases here should be transitioned from
      // `kResponseReceived` state, because `this` should have been servable
      // when `CreateRequestHandler()` was called, and thus the head/body should
      // remain valid (reflecting the successful `kResponseReceived`) and
      // `ForwardResponse()` should be called. Other `kFailed` cases shouldn't
      // reach here.
      //
      // TODO(crbug.com/1449360): we might want to revisit this behavior.

      // TODO(crbug.com/1483599): The code below is duplicated to investigate
      // the `load_state_` value on CHECK failure. Remove the duplicated code.
      CHECK(GetHead());
      CHECK(forward_body_);
      break;
    case LoadState::kCompleted:
      CHECK(GetHead());
      CHECK(forward_body_);
      break;
    case LoadState::kFailed:
      CHECK(GetHead());
      CHECK(forward_body_);
      break;

    case LoadState::kRedirectHandled:
      // For redirects, `ForwardResponse()` shouldn't be called at all, and the
      // head and body are both null.
      CHECK(!GetHead());
      CHECK(!forward_body_);
      break;

    case LoadState::kStarted:
    case LoadState::kFailedResponseReceived:
      // `CreateRequestHandler()` shouldn't be called for these non-servable
      // states.
      NOTREACHED();
      break;
  }

  RunEventQueue(client_id);

  // Basically `forward_body_` should have been moved out by `ForwardResponse()`
  // inside `RunEventQueue()`, but `forward_body_` can remain non-null here e.g.
  // when the serving clients are disconnected before `ForwardResponse()`.
  // Anyway clear `forward_body_` to ensure it is only valid/used in this
  // `BindAndStart()` -> `ForwardResponse()` scope.
  forward_body_.reset();
}

void PrefetchResponseReader::AddEventToQueue(
    base::RepeatingCallback<void(ServingUrlLoaderClientId)> callback) {
  // To avoid complexity and bugs, `AddEventToQueue()` and `RunEventQueue()` are
  // assumed non-reentrant. This should be OK because `callback` is just calling
  // URLLoaderClient mojo methods which are assumed to work asynchronously.
  CHECK_EQ(event_queue_status_, EventQueueStatus::kNotRunning);
  event_queue_status_ = EventQueueStatus::kRunning;

  // Dispatch `callback` to clients that are currently serving.
  //
  // If the event is added AFTER a client is added, then `callback` (and the
  // corresponding `URLLoaderClient` mojo method) is called directly here.

  // Iterate over a separate vector, just in case `serving_url_loader_clients_`
  // is mutated during iteration, which shouldn't occur (as we assume
  // non-reentrancy).
  std::vector<ServingUrlLoaderClientId> client_ids;
  for (auto it = serving_url_loader_clients_.begin();
       it != serving_url_loader_clients_.end(); ++it) {
    client_ids.push_back(it.id());
  }

  CHECK_EQ(serving_url_loader_clients_.size(), client_ids.size());
  for (auto client_id : client_ids) {
    callback.Run(client_id);
  }
  // Just roughly check that `serving_url_loader_clients_` seems unchanged.
  CHECK_EQ(serving_url_loader_clients_.size(), client_ids.size());

  // Queue `callback` to `event_queue_` for clients that might be added in the
  // future.
  //
  // If the event is added BEFORE a client is added, then `callback` is queued
  // to `event_queue_` here and will be called when the client is added (== when
  // `RunEventQueue()` is called).
  event_queue_.push_back(std::move(callback));

  event_queue_status_ = EventQueueStatus::kNotRunning;
}

void PrefetchResponseReader::RunEventQueue(ServingUrlLoaderClientId client_id) {
  CHECK_GT(event_queue_.size(), 0u);

  // Should be non-reentrant (see a comment in `AddEventToQueue()` above).
  CHECK_EQ(event_queue_status_, EventQueueStatus::kNotRunning);

  event_queue_status_ = EventQueueStatus::kRunning;
  for (const auto& callback : event_queue_) {
    callback.Run(client_id);
  }

  event_queue_status_ = EventQueueStatus::kNotRunning;
}

void PrefetchResponseReader::OnComplete(
    network::URLLoaderCompletionStatus completion_status) {
  // TODO(crbug.com/1484028): Remove this alias.
  auto load_state = load_state_;
  base::debug::Alias(&load_state);

  switch (load_state_) {
    case LoadState::kStarted:
      CHECK_NE(completion_status.error_code, net::OK);
      load_state_ = LoadState::kFailed;
      break;
    case LoadState::kResponseReceived:
      if (completion_status.error_code == net::OK) {
        load_state_ = LoadState::kCompleted;
      } else {
        load_state_ = LoadState::kFailed;
      }
      break;
    case LoadState::kFailedResponseReceived:
      load_state_ = LoadState::kFailed;
      break;
    case LoadState::kRedirectHandled:
      CHECK(false);
      break;
    case LoadState::kCompleted:
      CHECK(false);
      break;
    case LoadState::kFailed:
      CHECK(false);
      break;
  }

  CHECK(!response_complete_time_);
  CHECK(!completion_status_);
  response_complete_time_ = base::TimeTicks::Now();
  completion_status_ = completion_status;

  AddEventToQueue(
      base::BindRepeating(&PrefetchResponseReader::ForwardCompletionStatus,
                          base::Unretained(this)));
}

void PrefetchResponseReader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  CHECK(load_state_ == LoadState::kStarted ||
        load_state_ == LoadState::kResponseReceived ||
        load_state_ == LoadState::kFailedResponseReceived);

  AddEventToQueue(
      base::BindRepeating(&PrefetchResponseReader::ForwardEarlyHints,
                          base::Unretained(this), std::move(early_hints)));
}

void PrefetchResponseReader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  CHECK(load_state_ == LoadState::kStarted ||
        load_state_ == LoadState::kResponseReceived ||
        load_state_ == LoadState::kFailedResponseReceived);

  AddEventToQueue(
      base::BindRepeating(&PrefetchResponseReader::ForwardTransferSizeUpdate,
                          base::Unretained(this), transfer_size_diff));
}

void PrefetchResponseReader::HandleRedirect(
    PrefetchRedirectStatus redirect_status,
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  CHECK_EQ(load_state_, LoadState::kStarted);

  switch (redirect_status) {
    case PrefetchRedirectStatus::kFollow:
      load_state_ = LoadState::kRedirectHandled;
      // To record only one UMA per `PrefetchStreamingURLLoader`, skip UMA
      // recording if `this` is not the last `PrefetchResponseReader` of a
      // `PrefetchStreamingURLLoader`. This is to keep the existing behavior.
      should_record_metrics_ = false;
      break;
    case PrefetchRedirectStatus::kSwitchNetworkContext:
      load_state_ = LoadState::kRedirectHandled;
      break;

    case PrefetchRedirectStatus::kFail:
      load_state_ = LoadState::kFailed;
      failure_reason_ =
          PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect;
      // Do not add to the event queue on failure.
      return;
  }

  AddEventToQueue(base::BindRepeating(&PrefetchResponseReader::ForwardRedirect,
                                      base::Unretained(this), redirect_info,
                                      std::move(redirect_head)));
}

void PrefetchResponseReader::OnReceiveResponse(
    PrefetchStreamingURLLoaderStatus status,
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body) {
  CHECK_EQ(load_state_, LoadState::kStarted);
  CHECK(!head_);
  CHECK(head);
  CHECK(!body_);
  CHECK(serving_url_loader_clients_.empty());

  switch (status) {
    case PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody:
      load_state_ = LoadState::kResponseReceived;
      head->navigation_delivery_type =
          network::mojom::NavigationDeliveryType::kNavigationalPrefetch;
      CHECK(body);
      break;

    case PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHead:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHeaders:
    case PrefetchStreamingURLLoaderStatus::kFailedNon2XX:
    case PrefetchStreamingURLLoaderStatus::kFailedMIMENotSupported:
      load_state_ = LoadState::kFailedResponseReceived;
      failure_reason_ = status;
      // Discard `body` for non-servable cases, to keep the existing behavior
      // and also because `body` is not used.
      body.reset();
      break;

    case PrefetchStreamingURLLoaderStatus::kWaitingOnHead:
    case PrefetchStreamingURLLoaderStatus::kRedirected_DEPRECATED:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion:
    case PrefetchStreamingURLLoaderStatus::kFailedNetError:
    case PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed:
    case PrefetchStreamingURLLoaderStatus::kFollowRedirect_DEPRECATED:
    case PrefetchStreamingURLLoaderStatus::
        kPauseRedirectForEligibilityCheck_DEPRECATED:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect:
    case PrefetchStreamingURLLoaderStatus::
        kStopSwitchInNetworkContextForRedirect:
    case PrefetchStreamingURLLoaderStatus::
        kServedSwitchInNetworkContextForRedirect:
      NOTREACHED();
      break;
  }

  head_ = std::move(head);
  body_ = std::move(body);
  AddEventToQueue(base::BindRepeating(&PrefetchResponseReader::ForwardResponse,
                                      base::Unretained(this)));
}

void PrefetchResponseReader::ForwardCompletionStatus(
    ServingUrlLoaderClientId client_id) {
  CHECK(completion_status_);
  if (network::mojom::URLLoaderClient* client =
          serving_url_loader_clients_.Get(client_id)) {
    client->OnComplete(completion_status_.value());
  }
}

void PrefetchResponseReader::ForwardEarlyHints(
    const network::mojom::EarlyHintsPtr& early_hints,
    ServingUrlLoaderClientId client_id) {
  if (network::mojom::URLLoaderClient* client =
          serving_url_loader_clients_.Get(client_id)) {
    client->OnReceiveEarlyHints(early_hints->Clone());
  }
}

void PrefetchResponseReader::ForwardTransferSizeUpdate(
    int32_t transfer_size_diff,
    ServingUrlLoaderClientId client_id) {
  if (network::mojom::URLLoaderClient* client =
          serving_url_loader_clients_.Get(client_id)) {
    client->OnTransferSizeUpdated(transfer_size_diff);
  }
}

void PrefetchResponseReader::ForwardRedirect(
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHeadPtr& head,
    ServingUrlLoaderClientId client_id) {
  if (network::mojom::URLLoaderClient* client =
          serving_url_loader_clients_.Get(client_id)) {
    client->OnReceiveRedirect(redirect_info, head->Clone());
  }
}

void PrefetchResponseReader::ForwardResponse(
    ServingUrlLoaderClientId client_id) {
  CHECK(head_);
  CHECK(forward_body_);
  if (network::mojom::URLLoaderClient* client =
          serving_url_loader_clients_.Get(client_id)) {
    client->OnReceiveResponse(head_->Clone(), std::move(forward_body_),
                              absl::nullopt);
  }
}

void PrefetchResponseReader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  // If a URL loader provided to |NavigationURLLoaderImpl| to intercept triggers
  // a redirect, then it will be interrupted before |FollowRedirect| is called,
  // and instead interceptors are given a chance to intercept the navigation to
  // the redirect.
  NOTREACHED();
}

void PrefetchResponseReader::SetPriority(net::RequestPriority priority,
                                         int32_t intra_priority_value) {
  // Forward calls from the serving URL loader to the prefetch URL loader.
  if (streaming_url_loader_) {
    streaming_url_loader_->SetPriority(priority, intra_priority_value);
  }
}

void PrefetchResponseReader::PauseReadingBodyFromNet() {
  // Forward calls from the serving URL loader to the prefetch URL loader.
  if (streaming_url_loader_) {
    streaming_url_loader_->PauseReadingBodyFromNet();
  }
}

void PrefetchResponseReader::ResumeReadingBodyFromNet() {
  // Forward calls from the serving URL loader to the prefetch URL loader.
  if (streaming_url_loader_) {
    streaming_url_loader_->ResumeReadingBodyFromNet();
  }
}

PrefetchStreamingURLLoaderStatus PrefetchResponseReader::GetStatusForRecording()
    const {
  switch (load_state_) {
    case LoadState::kStarted:
      return PrefetchStreamingURLLoaderStatus::kWaitingOnHead;

    case LoadState::kRedirectHandled:
      if (served_before_completion_) {
        return PrefetchStreamingURLLoaderStatus::
            kServedSwitchInNetworkContextForRedirect;
      } else {
        return PrefetchStreamingURLLoaderStatus::
            kStopSwitchInNetworkContextForRedirect;
      }

    case LoadState::kResponseReceived:
      return PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody;

    case LoadState::kCompleted:
      if (served_before_completion_) {
        return PrefetchStreamingURLLoaderStatus::
            kSuccessfulServedBeforeCompletion;
      } else if (served_after_completion_) {
        return PrefetchStreamingURLLoaderStatus::
            kSuccessfulServedAfterCompletion;
      } else {
        return PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed;
      }

    case LoadState::kFailedResponseReceived:
    case LoadState::kFailed:
      if (failure_reason_) {
        // Only certain enum values can be set here.
        switch (*failure_reason_) {
          case PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy:
          case PrefetchStreamingURLLoaderStatus::kFailedInvalidHead:
          case PrefetchStreamingURLLoaderStatus::kFailedInvalidHeaders:
          case PrefetchStreamingURLLoaderStatus::kFailedNon2XX:
          case PrefetchStreamingURLLoaderStatus::kFailedMIMENotSupported:
          case PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect:
            break;
          default:
            NOTREACHED();
            break;
        }
        return *failure_reason_;
      } else if (served_before_completion_) {
        return PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed;
      } else {
        return PrefetchStreamingURLLoaderStatus::kFailedNetError;
      }
  }
}

}  // namespace content
