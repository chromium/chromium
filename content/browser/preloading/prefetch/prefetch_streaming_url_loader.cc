// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

PrefetchStreamingURLLoader::PrefetchStreamingURLLoader(
    OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
    OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback,
    OnPrefetchRedirectCallback on_prefetch_redirect_callback,
    base::OnceClosure on_received_head_callback)
    : on_prefetch_response_started_callback_(
          std::move(on_prefetch_response_started_callback)),
      on_prefetch_response_completed_callback_(
          std::move(on_prefetch_response_completed_callback)),
      on_prefetch_redirect_callback_(std::move(on_prefetch_redirect_callback)),
      on_received_head_callback_(std::move(on_received_head_callback)) {}

void PrefetchStreamingURLLoader::Start(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const network::ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::TimeDelta timeout_duration) {
  // Copying the ResourceRequest is currently necessary because the Mojo traits
  // for TrustedParams currently const_cast and then move the underlying
  // devtools_observer, rather than cloning it. The copy constructor for
  // TrustedParams, on the other hand, clones it correctly.
  //
  // This is a violation of const correctness which lead to a confusing bug
  // here. If that goes away, then this copy might not be necessary.
  url_loader_factory->CreateLoaderAndStart(
      prefetch_url_loader_.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
          network::mojom::kURLLoadOptionSniffMimeType |
          network::mojom::kURLLoadOptionSendSSLInfoForCertificateError,
      network::ResourceRequest(request),
      prefetch_url_loader_client_receiver_.BindNewPipeAndPassRemote(
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      net::MutableNetworkTrafficAnnotationTag(network_traffic_annotation));
  prefetch_url_loader_client_receiver_.set_disconnect_handler(base::BindOnce(
      &PrefetchStreamingURLLoader::DisconnectPrefetchURLLoaderMojo,
      weak_ptr_factory_.GetWeakPtr()));

  if (!timeout_duration.is_zero()) {
    timeout_timer_.Start(
        FROM_HERE, timeout_duration,
        base::BindOnce(&PrefetchStreamingURLLoader::OnComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       network::URLLoaderCompletionStatus(net::ERR_TIMED_OUT)));
  }
}

PrefetchStreamingURLLoader::~PrefetchStreamingURLLoader() = default;

// static
std::unique_ptr<PrefetchStreamingURLLoader> PrefetchStreamingURLLoader::Create(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const network::ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::TimeDelta timeout_duration,
    OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
    OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback,
    OnPrefetchRedirectCallback on_prefetch_redirect_callback,
    base::OnceClosure on_received_head_callback,
    base::WeakPtr<PrefetchResponseReader> response_reader) {
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          std::move(on_prefetch_response_started_callback),
          std::move(on_prefetch_response_completed_callback),
          std::move(on_prefetch_redirect_callback),
          std::move(on_received_head_callback));

  streaming_loader->SetResponseReader(std::move(response_reader));

  streaming_loader->Start(url_loader_factory, request,
                          network_traffic_annotation,
                          std::move(timeout_duration));

  return streaming_loader;
}

void PrefetchStreamingURLLoader::SetResponseReader(
    base::WeakPtr<PrefetchResponseReader> response_reader) {
  response_reader_ = std::move(response_reader);
  if (response_reader_) {
    response_reader_->SetStreamingURLLoader(GetWeakPtr());
  }
}

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

void PrefetchStreamingURLLoader::DisconnectPrefetchURLLoaderMojo() {
  prefetch_url_loader_.reset();
  prefetch_url_loader_client_receiver_.reset();
  prefetch_url_loader_disconnected_ = true;

  PostTaskToDeleteSelf();
}

void PrefetchStreamingURLLoader::PostTaskToDeleteSelfIfDisconnected() {
  if (prefetch_url_loader_disconnected_) {
    PostTaskToDeleteSelf();
  }
}

void PrefetchStreamingURLLoader::MakeSelfOwned(
    std::unique_ptr<PrefetchStreamingURLLoader> self) {
  self_pointer_ = std::move(self);
}

void PrefetchStreamingURLLoader::PostTaskToDeleteSelf() {
  if (!self_pointer_) {
    return;
  }

  // To avoid UAF bugs, post a separate task to delete this object.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(self_pointer_));
}

void PrefetchStreamingURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  if (response_reader_) {
    response_reader_->OnReceiveEarlyHints(std::move(early_hints));
  }
}

void PrefetchStreamingURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  // Cached metadata is not supported for prefetch.
  cached_metadata.reset();

  head->was_in_prefetch_cache = true;

  // Checks head to determine if the prefetch can be served.
  DCHECK(on_prefetch_response_started_callback_);
  PrefetchStreamingURLLoaderStatus status =
      std::move(on_prefetch_response_started_callback_).Run(head.get());

  // `head` and `body` are discarded if `response_reader_` is `nullptr`, because
  // it means the `PrefetchResponseReader` is deleted and thus we no longer
  // serve the prefetched result.
  if (response_reader_) {
    response_reader_->OnReceiveResponse(status, std::move(head),
                                        std::move(body));
  }

  if (on_received_head_callback_) {
    std::move(on_received_head_callback_).Run();
  }
}

void PrefetchStreamingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  DCHECK(on_prefetch_redirect_callback_);
  on_prefetch_redirect_callback_.Run(redirect_info, std::move(redirect_head));
}

void PrefetchStreamingURLLoader::HandleRedirect(
    PrefetchRedirectStatus redirect_status,
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  DCHECK(redirect_head);

  // If the prefetch_url_loader_ is no longer connected, mark this as failed.
  if (!prefetch_url_loader_) {
    redirect_status = PrefetchRedirectStatus::kFail;
  }

  switch (redirect_status) {
    case PrefetchRedirectStatus::kFollow:
      DCHECK(prefetch_url_loader_);
      prefetch_url_loader_->FollowRedirect(
          /*removed_headers=*/std::vector<std::string>(),
          /*modified_headers=*/net::HttpRequestHeaders(),
          /*modified_cors_exempt_headers=*/net::HttpRequestHeaders(),
          /*new_url=*/absl::nullopt);
      break;
    case PrefetchRedirectStatus::kSwitchNetworkContext:
      // The redirect requires a switch in network context, so the redirect will
      // be followed using a separate PrefetchStreamingURLLoader, and this url
      // loader will stop its request.
      DisconnectPrefetchURLLoaderMojo();
      timeout_timer_.AbandonAndStop();
      break;
    case PrefetchRedirectStatus::kFail:
      if (on_received_head_callback_) {
        std::move(on_received_head_callback_).Run();
      }
      break;
  }

  if (response_reader_) {
    response_reader_->HandleRedirect(redirect_status, redirect_info,
                                     std::move(redirect_head));
  }
}

void PrefetchStreamingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // Only handle GETs.
  NOTREACHED();
}

void PrefetchStreamingURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  if (response_reader_) {
    response_reader_->OnTransferSizeUpdated(transfer_size_diff);
  }
}

void PrefetchStreamingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  DisconnectPrefetchURLLoaderMojo();
  timeout_timer_.AbandonAndStop();

  if (response_reader_) {
    response_reader_->OnComplete(completion_status);
  }

  if (completion_status.error_code != net::OK) {
    // Note that we may have already started serving the prefetch if it was
    // marked as servable in |OnReceiveResponse|.
    if (on_received_head_callback_) {
      std::move(on_received_head_callback_).Run();
    }
  }

  std::move(on_prefetch_response_completed_callback_).Run(completion_status);
}

void PrefetchStreamingURLLoader::OnStartServing() {
  // Once the prefetch is served, stop the timeout timer.
  timeout_timer_.AbandonAndStop();
}

void PrefetchStreamingURLLoader::SetPriority(net::RequestPriority priority,
                                             int32_t intra_priority_value) {
  if (prefetch_url_loader_) {
    prefetch_url_loader_->SetPriority(priority, intra_priority_value);
  }
}

void PrefetchStreamingURLLoader::PauseReadingBodyFromNet() {
  if (prefetch_url_loader_) {
    prefetch_url_loader_->PauseReadingBodyFromNet();
  }
}

void PrefetchStreamingURLLoader::ResumeReadingBodyFromNet() {
  if (prefetch_url_loader_) {
    prefetch_url_loader_->ResumeReadingBodyFromNet();
  }
}

PrefetchResponseReader::PrefetchResponseReader() = default;

PrefetchResponseReader::~PrefetchResponseReader() {
  if (should_record_metrics_) {
    base::UmaHistogramEnumeration(
        "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus",
        GetStatusForRecording());
  }
}

void PrefetchResponseReader::SetStreamingURLLoader(
    base::WeakPtr<PrefetchStreamingURLLoader> streaming_url_loader) {
  DCHECK(!streaming_url_loader_);
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
  if (serving_url_loader_receiver_.is_bound()) {
    return;
  }

  // To avoid UAF bugs, post a separate task to possibly delete `this`.
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(self_pointer_));
}

void PrefetchResponseReader::OnServingURLLoaderMojoDisconnect() {
  serving_url_loader_receiver_.reset();
  serving_url_loader_client_.reset();
  MaybeReleaseSoonSelfPointer();
}

PrefetchResponseReader::RequestHandler
PrefetchResponseReader::CreateRequestHandler() {
  if (streaming_url_loader_) {
    streaming_url_loader_->OnStartServing();
  }

  return base::BindOnce(&PrefetchResponseReader::BindAndStart,
                        base::WrapRefCounted(this));
}

void PrefetchResponseReader::BindAndStart(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK(!serving_url_loader_receiver_.is_bound());
  DCHECK(!self_pointer_);
  self_pointer_ = base::WrapRefCounted(this);

  if (load_state_ == LoadState::kCompleted) {
    served_after_completion_ = true;
  } else {
    served_before_completion_ = true;
  }

  serving_url_loader_receiver_.Bind(std::move(receiver));
  serving_url_loader_receiver_.set_disconnect_handler(
      base::BindOnce(&PrefetchResponseReader::OnServingURLLoaderMojoDisconnect,
                     weak_ptr_factory_.GetWeakPtr()));
  serving_url_loader_client_.Bind(std::move(client));

  RunEventQueue();
}

void PrefetchResponseReader::AddEventToQueue(base::OnceClosure closure) {
  DCHECK(event_queue_status_ != EventQueueStatus::kFinished);

  event_queue_.emplace_back(std::move(closure));
}

void PrefetchResponseReader::RunEventQueue() {
  DCHECK(serving_url_loader_client_);
  DCHECK(event_queue_.size() > 0);
  DCHECK_EQ(event_queue_status_, EventQueueStatus::kNotStarted);

  event_queue_status_ = EventQueueStatus::kRunning;
  while (event_queue_.size() > 0) {
    auto event_itr = event_queue_.begin();
    std::move(*event_itr).Run();
    event_queue_.erase(event_itr);
  }
  event_queue_status_ = EventQueueStatus::kFinished;
}

void PrefetchResponseReader::OnComplete(
    network::URLLoaderCompletionStatus completion_status) {
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
    case LoadState::kCompleted:
    case LoadState::kFailed:
      CHECK(false);
      break;
  }

  DCHECK(!response_complete_time_);
  DCHECK(!completion_status_);
  response_complete_time_ = base::TimeTicks::Now();
  completion_status_ = completion_status;

  if (serving_url_loader_client_ &&
      event_queue_status_ == EventQueueStatus::kFinished) {
    ForwardCompletionStatus();
    return;
  }
  AddEventToQueue(
      base::BindOnce(&PrefetchResponseReader::ForwardCompletionStatus,
                     base::Unretained(this)));
}

void PrefetchResponseReader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  CHECK(load_state_ == LoadState::kStarted ||
        load_state_ == LoadState::kResponseReceived ||
        load_state_ == LoadState::kFailedResponseReceived);

  if (serving_url_loader_client_ &&
      event_queue_status_ == EventQueueStatus::kFinished) {
    ForwardEarlyHints(std::move(early_hints));
    return;
  }

  AddEventToQueue(base::BindOnce(&PrefetchResponseReader::ForwardEarlyHints,
                                 base::Unretained(this),
                                 std::move(early_hints)));
}

void PrefetchResponseReader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  CHECK(load_state_ == LoadState::kStarted ||
        load_state_ == LoadState::kResponseReceived ||
        load_state_ == LoadState::kFailedResponseReceived);

  if (serving_url_loader_client_ &&
      event_queue_status_ == EventQueueStatus::kFinished) {
    ForwardTransferSizeUpdate(transfer_size_diff);
    return;
  }

  AddEventToQueue(
      base::BindOnce(&PrefetchResponseReader::ForwardTransferSizeUpdate,
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

  DCHECK(event_queue_status_ == EventQueueStatus::kNotStarted);
  AddEventToQueue(base::BindOnce(&PrefetchResponseReader::ForwardRedirect,
                                 base::Unretained(this), redirect_info,
                                 std::move(redirect_head)));
}

void PrefetchResponseReader::OnReceiveResponse(
    PrefetchStreamingURLLoaderStatus status,
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body) {
  CHECK_EQ(load_state_, LoadState::kStarted);
  CHECK_EQ(event_queue_status_, EventQueueStatus::kNotStarted);
  CHECK(!head_);
  CHECK(head);

  switch (status) {
    case PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody:
      load_state_ = LoadState::kResponseReceived;
      head->navigation_delivery_type =
          network::mojom::NavigationDeliveryType::kNavigationalPrefetch;
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
  AddEventToQueue(base::BindOnce(&PrefetchResponseReader::ForwardResponse,
                                 base::Unretained(this), std::move(body)));
}

void PrefetchResponseReader::ForwardCompletionStatus() {
  DCHECK(serving_url_loader_client_);
  DCHECK(completion_status_);
  serving_url_loader_client_->OnComplete(completion_status_.value());
}

void PrefetchResponseReader::ForwardEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK(serving_url_loader_client_);
  serving_url_loader_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void PrefetchResponseReader::ForwardTransferSizeUpdate(
    int32_t transfer_size_diff) {
  DCHECK(serving_url_loader_client_);
  serving_url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PrefetchResponseReader::ForwardRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK(serving_url_loader_client_);
  serving_url_loader_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void PrefetchResponseReader::ForwardResponse(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(serving_url_loader_client_);
  DCHECK(head_);
  DCHECK(body);
  serving_url_loader_client_->OnReceiveResponse(head_->Clone(), std::move(body),
                                                absl::nullopt);
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
