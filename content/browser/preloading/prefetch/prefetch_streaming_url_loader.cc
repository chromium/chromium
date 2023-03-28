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
    std::unique_ptr<network::ResourceRequest> request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::TimeDelta timeout_duration,
    OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
    OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback,
    OnPrefetchRedirectCallback on_prefetch_redirect_callback)
    : on_prefetch_response_started_callback_(
          std::move(on_prefetch_response_started_callback)),
      on_prefetch_response_completed_callback_(
          std::move(on_prefetch_response_completed_callback)),
      on_prefetch_redirect_callback_(std::move(on_prefetch_redirect_callback)) {
  url_loader_factory->CreateLoaderAndStart(
      prefetch_url_loader_.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
          network::mojom::kURLLoadOptionSniffMimeType |
          network::mojom::kURLLoadOptionSendSSLInfoForCertificateError,
      *request,
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

void PrefetchStreamingURLLoader::SetOnReceivedHeadCallback(
    base::OnceClosure on_received_head_callback) {
  on_received_head_callback_ = std::move(on_received_head_callback);
}

bool PrefetchStreamingURLLoader::Servable(
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
    case PrefetchStreamingURLLoaderStatus::kPauseRedirectForEligibilityCheck:
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
      NOTREACHED();
      return true;
  }
}

void PrefetchStreamingURLLoader::DisconnectPrefetchURLLoaderMojo() {
  prefetch_url_loader_.reset();
  prefetch_url_loader_client_receiver_.reset();
  prefetch_url_loader_disconnected_ = true;

  if (serving_url_loader_disconnected_) {
    PostTaskToDeleteSelf();
  }
}

void PrefetchStreamingURLLoader::OnServingURLLoaderMojoDisconnect() {
  serving_url_loader_receiver_.reset();
  serving_url_loader_client_.reset();
  serving_url_loader_disconnected_ = true;

  if (prefetch_url_loader_disconnected_) {
    PostTaskToDeleteSelf();
  }
}

void PrefetchStreamingURLLoader::MakeSelfOwnedAndDeleteSoon(
    std::unique_ptr<PrefetchStreamingURLLoader> self) {
  self_pointer_ = std::move(self);
  PostTaskToDeleteSelf();
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
  if (serving_url_loader_client_ &&
      event_queue_status_ == EventQueueStatus::kFinished) {
    ForwardEarlyHints(std::move(early_hints));
    return;
  }

  AddEventToQueue(
      base::BindOnce(&PrefetchStreamingURLLoader::ForwardEarlyHints,
                     base::Unretained(this), std::move(early_hints)),
      /*pause_after_event=*/false);
}

void PrefetchStreamingURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  // Cached metadata is not supported for prefetch.
  cached_metadata.reset();

  head_ = std::move(head);
  head_->was_in_prefetch_cache = true;

  // Checks head to determine if the prefetch can be served.
  DCHECK(on_prefetch_response_started_callback_);
  status_ = std::move(on_prefetch_response_started_callback_).Run(head_.get());

  // Update servable_ based on the returned status_
  switch (status_) {
    case PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody:
      servable_ = true;
      break;
    case PrefetchStreamingURLLoaderStatus::kPrefetchWasDecoy:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHead:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidHeaders:
    case PrefetchStreamingURLLoaderStatus::kFailedNon2XX:
    case PrefetchStreamingURLLoaderStatus::kFailedMIMENotSupported:
      servable_ = false;
      break;
    case PrefetchStreamingURLLoaderStatus::kWaitingOnHead:
    case PrefetchStreamingURLLoaderStatus::kRedirected_DEPRECATED:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion:
    case PrefetchStreamingURLLoaderStatus::kFailedNetError:
    case PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed:
    case PrefetchStreamingURLLoaderStatus::kFollowRedirect:
    case PrefetchStreamingURLLoaderStatus::kPauseRedirectForEligibilityCheck:
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect:
      NOTREACHED();
      break;
  }

  if (!servable_) {
    if (on_received_head_callback_) {
      std::move(on_received_head_callback_).Run();
    }

    return;
  }

  head_->navigation_delivery_type =
      network::mojom::NavigationDeliveryType::kNavigationalPrefetch;
  body_ = std::move(body);

  if (on_received_head_callback_) {
    std::move(on_received_head_callback_).Run();
  }

  DCHECK(event_queue_status_ == EventQueueStatus::kNotStarted);
  AddEventToQueue(base::BindOnce(&PrefetchStreamingURLLoader::ForwardResponse,
                                 base::Unretained(this)),
                  /*pause_after_event=*/false);
}

void PrefetchStreamingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK(on_prefetch_redirect_callback_);
  DCHECK(!redirect_head_);

  redirect_info_ = redirect_info;
  redirect_head_ = std::move(head);

  HandleRedirect(
      on_prefetch_redirect_callback_.Run(redirect_info, *redirect_head_.get()));
}

void PrefetchStreamingURLLoader::OnEligibilityCheckForRedirectComplete(
    bool is_eligible) {
  DCHECK(status_ ==
         PrefetchStreamingURLLoaderStatus::kPauseRedirectForEligibilityCheck);
  HandleRedirect(
      is_eligible ? PrefetchStreamingURLLoaderStatus::kFollowRedirect
                  : PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect);
}

void PrefetchStreamingURLLoader::HandleRedirect(
    PrefetchStreamingURLLoaderStatus new_status) {
  DCHECK(redirect_head_);

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

      DCHECK(event_queue_status_ == EventQueueStatus::kNotStarted);
      AddEventToQueue(
          base::BindOnce(&PrefetchStreamingURLLoader::ForwardRedirect,
                         base::Unretained(this), redirect_info_,
                         std::move(redirect_head_)),
          /*pause_after_event=*/true);
      break;
    case PrefetchStreamingURLLoaderStatus::kPauseRedirectForEligibilityCheck:
      // The eligibility check is still running on the redirect URL. Once it is
      // completed, then |OnEligibilityCheckForRedirectComplete| will be called
      // with the result, and then either the redirect will be followed or the
      // URL loader will stop.
      break;
    case PrefetchStreamingURLLoaderStatus::kFailedInvalidRedirect:
      servable_ = false;
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
  if (serving_url_loader_client_ &&
      event_queue_status_ == EventQueueStatus::kFinished) {
    ForwardTransferSizeUpdate(transfer_size_diff);
    return;
  }

  AddEventToQueue(
      base::BindOnce(&PrefetchStreamingURLLoader::ForwardTransferSizeUpdate,
                     base::Unretained(this), transfer_size_diff),
      /*pause_after_event=*/false);
}

void PrefetchStreamingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  DisconnectPrefetchURLLoaderMojo();
  timeout_timer_.AbandonAndStop();

  completion_status_ = completion_status;
  response_complete_time_ = base::TimeTicks::Now();

  if (status_ == PrefetchStreamingURLLoaderStatus::kWaitingOnHead ||
      status_ == PrefetchStreamingURLLoaderStatus::kHeadReceivedWaitingOnBody) {
    status_ = completion_status_->error_code == net::OK
                  ? PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed
                  : PrefetchStreamingURLLoaderStatus::kFailedNetError;
  } else if (status_ == PrefetchStreamingURLLoaderStatus::
                            kSuccessfulServedBeforeCompletion &&
             completion_status_->error_code != net::OK) {
    status_ = PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed;
  }

  if (completion_status_->error_code != net::OK) {
    // Note that we may have already started serving the prefetch if it was
    // marked as servable in |OnReceiveResponse|.
    servable_ = false;
  }

  std::move(on_prefetch_response_completed_callback_)
      .Run(completion_status_.value());

  if (serving_url_loader_client_ &&
      event_queue_status_ == EventQueueStatus::kFinished) {
    ForwardCompletionStatus();
    return;
  }
  AddEventToQueue(
      base::BindOnce(&PrefetchStreamingURLLoader::ForwardCompletionStatus,
                     base::Unretained(this)),
      /*pause_after_event=*/false);
}

PrefetchStreamingURLLoader::RequestHandler
PrefetchStreamingURLLoader::ServingFinalResponseHandler(
    std::unique_ptr<PrefetchStreamingURLLoader> self) {
  DCHECK(self);
  DCHECK(IsReadyToServeFinalResponse());
  return base::BindOnce(&PrefetchStreamingURLLoader::BindAndStart,
                        weak_ptr_factory_.GetWeakPtr(), std::move(self));
}

PrefetchStreamingURLLoader::RequestHandler
PrefetchStreamingURLLoader::ServingRedirectHandler() {
  DCHECK(!IsReadyToServeFinalResponse());
  return base::BindOnce(&PrefetchStreamingURLLoader::BindAndStart,
                        weak_ptr_factory_.GetWeakPtr(), nullptr);
}

void PrefetchStreamingURLLoader::BindAndStart(
    std::unique_ptr<PrefetchStreamingURLLoader> self,
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK(servable_);
  DCHECK(!serving_url_loader_receiver_.is_bound());
  DCHECK(!self || self.get() == this);

  // Once the prefetch is served, stop the timeout timer.
  timeout_timer_.AbandonAndStop();

  status_ =
      completion_status_.has_value()
          ? PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion
          : PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion;

  // If the final response is ready to be served, then make self owned, and
  // delete self once serving the prefetch is finished.
  if (self) {
    self_pointer_ = std::move(self);
  }

  serving_url_loader_disconnected_ = false;
  serving_url_loader_receiver_.Bind(std::move(receiver));
  serving_url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &PrefetchStreamingURLLoader::OnServingURLLoaderMojoDisconnect,
      weak_ptr_factory_.GetWeakPtr()));
  serving_url_loader_client_.Bind(std::move(client));

  RunEventQueue();
}

bool PrefetchStreamingURLLoader::IsReadyToServeFinalResponse() const {
  for (const auto& event : event_queue_) {
    if (event.second) {
      return false;
    }
  }
  return true;
}

void PrefetchStreamingURLLoader::AddEventToQueue(base::OnceClosure closure,
                                                 bool pause_after_event) {
  DCHECK(event_queue_status_ != EventQueueStatus::kFinished);

  event_queue_.emplace_back(std::move(closure), pause_after_event);
}

void PrefetchStreamingURLLoader::RunEventQueue() {
  DCHECK(serving_url_loader_client_);
  DCHECK(event_queue_.size() > 0);
  DCHECK(event_queue_status_ == EventQueueStatus::kNotStarted ||
         event_queue_status_ == EventQueueStatus::kPaused);

  event_queue_status_ = EventQueueStatus::kRunning;
  while (event_queue_.size() > 0) {
    auto event_itr = event_queue_.begin();

    base::OnceClosure& event_closure = event_itr->first;
    bool pause_after_event = event_itr->second;

    std::move(event_closure).Run();

    event_queue_.erase(event_itr);
    if (pause_after_event) {
      event_queue_status_ = EventQueueStatus::kPaused;
      return;
    }
  }
  event_queue_status_ = EventQueueStatus::kFinished;
}

void PrefetchStreamingURLLoader::ForwardCompletionStatus() {
  DCHECK(serving_url_loader_client_);
  DCHECK(completion_status_);
  serving_url_loader_client_->OnComplete(completion_status_.value());
}

void PrefetchStreamingURLLoader::ForwardEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK(serving_url_loader_client_);
  serving_url_loader_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void PrefetchStreamingURLLoader::ForwardTransferSizeUpdate(
    int32_t transfer_size_diff) {
  DCHECK(serving_url_loader_client_);
  serving_url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PrefetchStreamingURLLoader::ForwardRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK(serving_url_loader_client_);
  serving_url_loader_client_->OnReceiveRedirect(redirect_info, std::move(head));
}

void PrefetchStreamingURLLoader::ForwardResponse() {
  DCHECK(serving_url_loader_client_);
  DCHECK(head_);
  DCHECK(body_);
  serving_url_loader_client_->OnReceiveResponse(
      head_->Clone(), std::move(body_), absl::nullopt);
}

void PrefetchStreamingURLLoader::FollowRedirect(
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

void PrefetchStreamingURLLoader::SetPriority(net::RequestPriority priority,
                                             int32_t intra_priority_value) {
  // Forward calls from the serving URL loader to the prefetch URL loader.
  if (prefetch_url_loader_) {
    prefetch_url_loader_->SetPriority(priority, intra_priority_value);
  }
}

void PrefetchStreamingURLLoader::PauseReadingBodyFromNet() {
  // Forward calls from the serving URL loader to the prefetch URL loader.
  if (prefetch_url_loader_) {
    prefetch_url_loader_->PauseReadingBodyFromNet();
  }
}

void PrefetchStreamingURLLoader::ResumeReadingBodyFromNet() {
  // Forward calls from the serving URL loader to the prefetch URL loader.
  if (prefetch_url_loader_) {
    prefetch_url_loader_->ResumeReadingBodyFromNet();
  }
}

}  // namespace content
