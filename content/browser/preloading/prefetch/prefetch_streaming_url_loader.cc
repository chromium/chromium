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
    network::mojom::URLLoaderFactory* url_loader_factory,
    const network::ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::TimeDelta timeout_duration,
    OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
    OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback,
    OnPrefetchRedirectCallback on_prefetch_redirect_callback,
    base::OnceClosure on_received_head_callback,
    base::WeakPtr<PrefetchResponseReader> response_reader)
    : on_prefetch_response_started_callback_(
          std::move(on_prefetch_response_started_callback)),
      on_prefetch_response_completed_callback_(
          std::move(on_prefetch_response_completed_callback)),
      on_prefetch_redirect_callback_(std::move(on_prefetch_redirect_callback)),
      on_received_head_callback_(std::move(on_received_head_callback)) {
  SetResponseReader(std::move(response_reader));

  url_loader_factory->CreateLoaderAndStart(
      prefetch_url_loader_.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
          network::mojom::kURLLoadOptionSniffMimeType |
          network::mojom::kURLLoadOptionSendSSLInfoForCertificateError,
      request,
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

PrefetchStreamingURLLoader::~PrefetchStreamingURLLoader() {
  base::UmaHistogramEnumeration(
      "PrefetchProxy.Prefetch.StreamingURLLoaderFinalStatus", status_);
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
  // If the response hasn't been received yet (meaning response_complete_time_
  // is absl::nullopt), we can still serve the prefetch (depending on |head_|).
  return servable_ &&
         (!response_complete_time_.has_value() ||
          base::TimeTicks::Now() <
              response_complete_time_.value() + cacheable_duration);
}

bool PrefetchStreamingURLLoader::Failed() const {
  switch (status_) {
    case PrefetchStreamingURLLoaderStatus::kWaitingOnHead:
    case PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion:
    case PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy:
    case PrefetchStreamingURLLoaderStatus::kFollowRedirect:
    case PrefetchStreamingURLLoaderStatus::
        kStopSwitchInNetworkContextForRedirect:
    case PrefetchStreamingURLLoaderStatus::
        kServedSwitchInNetworkContextForRedirect:
      return false;
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHead:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHeaders:
    case PrefetchStreamingURLLoaderStatus::kFailedNon2XX:
    case PrefetchStreamingURLLoaderStatus::kFailedMIMENotSupported:
    case PrefetchStreamingURLLoaderStatus::kFailedNetError:
    case PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect:
      return true;
    case PrefetchStreamingURLLoaderStatus::kRedirected_DEPRECATED:
    case PrefetchStreamingURLLoaderStatus::
        kPauseRedirectForEligibilityCheck_DEPRECATED:
      NOTREACHED();
      return true;
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
  status_ = std::move(on_prefetch_response_started_callback_).Run(head.get());

  bool servable = false;
  switch (status_) {
    case PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody:
      servable = true;
      break;
    case PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHead:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHeaders:
    case PrefetchStreamingURLLoaderStatus::kFailedNon2XX:
    case PrefetchStreamingURLLoaderStatus::kFailedMIMENotSupported:
      break;
    case PrefetchStreamingURLLoaderStatus::kWaitingOnHead:
    case PrefetchStreamingURLLoaderStatus::kRedirected_DEPRECATED:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion:
    case PrefetchStreamingURLLoaderStatus::kFailedNetError:
    case PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed:
    case PrefetchStreamingURLLoaderStatus::kFollowRedirect:
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

  if (servable) {
    head->navigation_delivery_type =
        network::mojom::NavigationDeliveryType::kNavigationalPrefetch;
  } else {
    // Discard `body` for non-servable cases, to keep the existing behavior and
    // also because `body` is not used.
    body.reset();
  }

  // `head` and `body` are discarded if `response_reader_` is `nullptr`, because
  // it means the `PrefetchResponseReader` is deleted and thus we no longer
  // serve the prefetched result.
  if (response_reader_) {
    response_reader_->OnReceiveResponse(servable, std::move(head),
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
    PrefetchStreamingURLLoaderStatus new_status,
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  DCHECK(redirect_head);

  // If the prefetch_url_loader_ is no longer connected, mark this as failed.
  if (!prefetch_url_loader_) {
    new_status = PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect;
  }

  status_ = new_status;
  switch (status_) {
    case PrefetchStreamingURLLoaderStatus::kFollowRedirect:
      DCHECK(prefetch_url_loader_);
      prefetch_url_loader_->FollowRedirect(
          /*removed_headers=*/std::vector<std::string>(),
          /*modified_headers=*/net::HttpRequestHeaders(),
          /*modified_cors_exempt_headers=*/net::HttpRequestHeaders(),
          /*new_url=*/absl::nullopt);

      if (response_reader_) {
        response_reader_->HandleRedirect(redirect_info,
                                         std::move(redirect_head));
      }
      break;
    case PrefetchStreamingURLLoaderStatus::
        kStopSwitchInNetworkContextForRedirect:
      // The redirect requires a switch in network context, so the redirect will
      // be followed using a separate PrefetchStreamingURLLoader, and this url
      // loader will stop its request.
      DisconnectPrefetchURLLoaderMojo();
      timeout_timer_.AbandonAndStop();

      if (response_reader_) {
        response_reader_->HandleRedirect(redirect_info,
                                         std::move(redirect_head));
      }
      break;
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect:
      if (on_received_head_callback_) {
        std::move(on_received_head_callback_).Run();
      }
      break;
    case PrefetchStreamingURLLoaderStatus::kWaitingOnHead:
    case PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody:
    case PrefetchStreamingURLLoaderStatus::kRedirected_DEPRECATED:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion:
    case PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHead:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHeaders:
    case PrefetchStreamingURLLoaderStatus::kFailedNon2XX:
    case PrefetchStreamingURLLoaderStatus::kFailedMIMENotSupported:
    case PrefetchStreamingURLLoaderStatus::kFailedNetError:
    case PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed:
    case PrefetchStreamingURLLoaderStatus::
        kPauseRedirectForEligibilityCheck_DEPRECATED:
    case PrefetchStreamingURLLoaderStatus::
        kServedSwitchInNetworkContextForRedirect:
      NOTREACHED();
      break;
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

  if (status_ == PrefetchStreamingURLLoaderStatus::kWaitingOnHead ||
      status_ == PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody) {
    status_ = completion_status.error_code == net::OK
                  ? PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed
                  : PrefetchStreamingURLLoaderStatus::kFailedNetError;
  } else if (status_ == PrefetchStreamingURLLoaderStatus::
                            kSuccessfulServedBeforeCompletion &&
             completion_status.error_code != net::OK) {
    status_ = PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed;
  }

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

  if (status_ == PrefetchStreamingURLLoaderStatus::
                     kStopSwitchInNetworkContextForRedirect) {
    status_ = PrefetchStreamingURLLoaderStatus::
        kServedSwitchInNetworkContextForRedirect;
  } else if (response_reader_ &&
             response_reader_->GetCompletionStatus().has_value()) {
    status_ =
        PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion;
  } else {
    status_ =
        PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion;
  }
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

PrefetchResponseReader::~PrefetchResponseReader() = default;

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
  DCHECK(!last_event_added_);
  DCHECK(!response_complete_time_);
  DCHECK(!completion_status_);

  last_event_added_ = true;
  response_complete_time_ = base::TimeTicks::Now();
  completion_status_ = completion_status;

  if (completion_status.error_code != net::OK) {
    servable_ = false;
  }

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
  DCHECK(!last_event_added_);
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
  DCHECK(!last_event_added_);
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
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  DCHECK(!last_event_added_);
  // Because we always switch to a new `PrefetchResponseReader` on redirects,
  // this redirect event is the last event of `this`.
  last_event_added_ = true;

  DCHECK(event_queue_status_ == EventQueueStatus::kNotStarted);
  AddEventToQueue(base::BindOnce(&PrefetchResponseReader::ForwardRedirect,
                                 base::Unretained(this), redirect_info,
                                 std::move(redirect_head)));
}

void PrefetchResponseReader::OnReceiveResponse(
    bool servable,
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(!last_event_added_);
  DCHECK(event_queue_status_ == EventQueueStatus::kNotStarted);
  DCHECK(!servable_);
  DCHECK(!head_);
  DCHECK(head);

  servable_ = servable;
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

}  // namespace content
