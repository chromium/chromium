// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_response_reader.h"

#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"
#include "net/http/http_cookie_indices.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

namespace {

PrefetchStreamingURLLoaderStatus
GetStatusForRecordingFromErrorOnResponseReceived(
    PrefetchErrorOnResponseReceived status) {
  switch (status) {
    case PrefetchErrorOnResponseReceived::kPrefetchWasDecoy:
      return PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy;
    case PrefetchErrorOnResponseReceived::kFailedInvalidHead:
      return PrefetchStreamingURLLoaderStatus::kFailedInvalidHead;
    case PrefetchErrorOnResponseReceived::kFailedInvalidHeaders:
      return PrefetchStreamingURLLoaderStatus::kFailedInvalidHeaders;
    case PrefetchErrorOnResponseReceived::kFailedNon2XX:
      return PrefetchStreamingURLLoaderStatus::kFailedNon2XX;
    case PrefetchErrorOnResponseReceived::kFailedMIMENotSupported:
      return PrefetchStreamingURLLoaderStatus::kFailedMIMENotSupported;
  }
}

}  // namespace

bool PrefetchResponseReader::Servable(
    base::TimeDelta cacheable_duration) const {
  switch (load_state_) {
    case LoadState::kResponseReceived:
      // If the response hasn't been completed yet, we can still serve the
      // prefetch (depending on |head_|).
      CHECK(!response_complete_time_);
      return true;

    case LoadState::kCompleted:
      // Prefetch is servable as long as it is fresh.
      CHECK(response_complete_time_);
      return base::TimeTicks::Now() <
             response_complete_time_.value() + cacheable_duration;

    case LoadState::kStarted:
    case LoadState::kRedirectHandled:
    case LoadState::kFailedResponseReceived:
    case LoadState::kFailedRedirect:
      CHECK(!response_complete_time_)
          << "LoadState: " << static_cast<int>(load_state_);
      return false;

    case LoadState::kFailed:
      CHECK(response_complete_time_);
      return false;
  }
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
    case LoadState::kFailedRedirect:
      return false;
  }
}

bool PrefetchResponseReader::VariesOnCookieIndices() const {
  return cookie_indices_.has_value();
}

bool PrefetchResponseReader::MatchesCookieIndices(
    base::span<const std::pair<std::string, std::string>> cookies) const {
  CHECK(cookie_indices_.has_value());
  net::CookieIndicesHash hash =
      net::HashCookieIndices(cookie_indices_->cookie_names, cookies);
  return hash == cookie_indices_->expected_hash;
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
  mojo::ScopedDataPipeConsumerHandle body;

  // Returns a null handler if some checks fail here.
  // This is a subset of the checks in `BindAndStart()`, but not identical,
  // because `load_state_` can be transitioned between the two methods. Still
  // the CHECKs in `BindAndStart()` should pass even when `load_state_` is
  // transitioned.
  switch (load_state_) {
    case LoadState::kResponseReceived:
    case LoadState::kCompleted:
    case LoadState::kFailed:
      if (base::FeatureList::IsEnabled(features::kPrefetchReusable)) {
        if (body_tee_) {
          body = body_tee_->Clone();
        }
      } else {
        body = std::move(body_);
      }
      if (!body) {
        // This might be because `CreateRequestHandler()` is called for the
        // second time.
        return {};
      }
      break;

    case LoadState::kRedirectHandled:
      CHECK(!body_);
      CHECK(!body_tee_);
      break;

    case LoadState::kStarted:
    case LoadState::kFailedResponseReceived:
    case LoadState::kFailedRedirect:
      return {};
  }

  if (streaming_url_loader_) {
    streaming_url_loader_->OnStartServing();
  }

  return base::BindOnce(&PrefetchResponseReader::BindAndStart,
                        base::WrapRefCounted(this), std::move(body));
}

void PrefetchResponseReader::BindAndStart(
    mojo::ScopedDataPipeConsumerHandle body,
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  if (!base::FeatureList::IsEnabled(features::kPrefetchReusable)) {
    // Only one client is allowed if the feature is disabled.
    CHECK(serving_url_loader_clients_.empty());
  }

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
      // TODO(crbug.com/40064891): we might want to revisit this behavior.

      // TODO(crbug.com/40072532): The code below is duplicated to investigate
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
    case LoadState::kFailedRedirect:
      // `CreateRequestHandler()` shouldn't be called for these non-servable
      // states.
      NOTREACHED_IN_MIGRATION();
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
  // TODO(crbug.com/40072670): Remove this alias.
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
    case LoadState::kFailedRedirect:
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
      load_state_ = LoadState::kFailedRedirect;
      // Do not add to the event queue on failure.
      return;
  }

  // Store away the info we want, then clear the request cookies before we
  // potentially forward them to any client.
  StoreInfoFromResponseHead(*redirect_head);
  redirect_head->request_cookies.clear();

  AddEventToQueue(base::BindRepeating(&PrefetchResponseReader::ForwardRedirect,
                                      base::Unretained(this), redirect_info,
                                      std::move(redirect_head)));
}

void PrefetchResponseReader::OnReceiveResponse(
    std::optional<PrefetchErrorOnResponseReceived> error,
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body) {
  CHECK_EQ(load_state_, LoadState::kStarted);
  CHECK(!head_);
  CHECK(head);
  CHECK(!body_);
  CHECK(!body_tee_);
  CHECK(serving_url_loader_clients_.empty());

  if (!error) {
    load_state_ = LoadState::kResponseReceived;
    head->navigation_delivery_type =
        network::mojom::NavigationDeliveryType::kNavigationalPrefetch;
    CHECK(body);
  } else {
    failure_reason_ = std::move(error);
    load_state_ = LoadState::kFailedResponseReceived;
    // Discard `body` for non-servable cases, to keep the existing behavior
    // and also because `body` is not used.
    body.reset();
  }

  // Store away the info we want, then clear the request cookies before we
  // potentially forward them to any client.
  StoreInfoFromResponseHead(*head);
  head->request_cookies.clear();

  head_ = std::move(head);
  if (base::FeatureList::IsEnabled(features::kPrefetchReusable)) {
    body_tee_ = base::MakeRefCounted<PrefetchDataPipeTee>(
        std::move(body), features::kPrefetchReusableBodySizeLimit.Get());
  } else {
    body_ = std::move(body);
  }
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
                              std::nullopt);
  }
}

void PrefetchResponseReader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  // If a URL loader provided to |NavigationURLLoaderImpl| to intercept triggers
  // a redirect, then it will be interrupted before |FollowRedirect| is called,
  // and instead interceptors are given a chance to intercept the navigation to
  // the redirect.
  NOTREACHED_IN_MIGRATION();
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

    case LoadState::kFailedRedirect:
      return PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect;

    case LoadState::kFailedResponseReceived:
    case LoadState::kFailed:
      if (failure_reason_) {
        return GetStatusForRecordingFromErrorOnResponseReceived(
            *failure_reason_);
      } else if (served_before_completion_) {
        return PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed;
      } else {
        return PrefetchStreamingURLLoaderStatus::kFailedNetError;
      }
  }
}

void PrefetchResponseReader::StoreInfoFromResponseHead(
    const network::mojom::URLResponseHead& head) {
  // Responses that don't have headers generated by the network service don't
  // have anything to store.
  if (!head.headers || !head.parsed_headers) {
    return;
  }
  CHECK(!cookie_indices_)
      << "This shouldn't happen more than once per PrefetchResponseReader.";
  size_t iter = 0;
  std::string request_header;
  bool vary_on_cookie = false;
  while (head.headers->EnumerateHeader(&iter, "vary", &request_header)) {
    if (request_header == "*" ||
        base::EqualsCaseInsensitiveASCII(request_header, "cookie")) {
      vary_on_cookie = true;
      break;
    }
  }
  if (vary_on_cookie && head.parsed_headers->cookie_indices.has_value()) {
    auto& indices = cookie_indices_.emplace();
    indices.cookie_names = *head.parsed_headers->cookie_indices;
    base::ranges::sort(indices.cookie_names);
    indices.cookie_names.erase(base::ranges::unique(indices.cookie_names),
                               indices.cookie_names.end());
    indices.cookie_names.shrink_to_fit();
    indices.expected_hash =
        net::HashCookieIndices(indices.cookie_names, head.request_cookies);
  }
}

PrefetchResponseReader::CookieIndicesInfo::CookieIndicesInfo() = default;
PrefetchResponseReader::CookieIndicesInfo::~CookieIndicesInfo() = default;

}  // namespace content
