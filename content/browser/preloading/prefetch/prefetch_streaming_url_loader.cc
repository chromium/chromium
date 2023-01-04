// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
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
          base::ThreadTaskRunnerHandle::Get()),
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
  base::SequencedTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                     std::move(self_pointer_));
}

void PrefetchStreamingURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  if (serving_url_loader_client_) {
    serving_url_loader_client_->OnReceiveEarlyHints(std::move(early_hints));
    return;
  }

  event_queue_.push_back(
      base::BindOnce(&PrefetchStreamingURLLoader::OnReceiveEarlyHints,
                     base::Unretained(this), std::move(early_hints)));
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
    case PrefetchStreamingURLLoaderStatus::kRedirected:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulNotServed:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion:
    case PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion:
    case PrefetchStreamingURLLoaderStatus::kFailedNetError:
    case PrefetchStreamingURLLoaderStatus::kFailedNetErrorButServed:
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
}

void PrefetchStreamingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  servable_ = false;
  status_ = PrefetchStreamingURLLoaderStatus::kRedirected;

  // Redirects are currently not supported by prefetch, so is just to inform
  // the owner of the callback of the redirect.
  std::vector<std::string> removed_headers;
  DCHECK(on_prefetch_redirect_callback_);
  on_prefetch_redirect_callback_.Run(redirect_info, *head.get(),
                                     &removed_headers);

  if (on_received_head_callback_) {
    std::move(on_received_head_callback_).Run();
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
  if (serving_url_loader_client_) {
    serving_url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
    return;
  }

  event_queue_.push_back(
      base::BindOnce(&PrefetchStreamingURLLoader::OnTransferSizeUpdated,
                     base::Unretained(this), transfer_size_diff));
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

  if (serving_url_loader_client_) {
    ForwardCompletionStatus();
    return;
  }
  event_queue_.push_back(
      base::BindOnce(&PrefetchStreamingURLLoader::ForwardCompletionStatus,
                     base::Unretained(this)));
}

void PrefetchStreamingURLLoader::ForwardCompletionStatus() {
  DCHECK(serving_url_loader_client_);
  DCHECK(completion_status_);
  serving_url_loader_client_->OnComplete(completion_status_.value());
}

PrefetchStreamingURLLoader::RequestHandler
PrefetchStreamingURLLoader::ServingResponseHandler(
    std::unique_ptr<PrefetchStreamingURLLoader> self) {
  return base::BindOnce(&PrefetchStreamingURLLoader::BindAndStart,
                        weak_ptr_factory_.GetWeakPtr(), std::move(self));
}

void PrefetchStreamingURLLoader::BindAndStart(
    std::unique_ptr<PrefetchStreamingURLLoader> self,
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK(servable_);
  DCHECK(!serving_url_loader_receiver_.is_bound());
  DCHECK(self.get() == this);

  status_ =
      completion_status_.has_value()
          ? PrefetchStreamingURLLoaderStatus::kSuccessfulServedAfterCompletion
          : PrefetchStreamingURLLoaderStatus::kSuccessfulServedBeforeCompletion;

  // Make this self owned. This will delete itself once prefetching and serving
  // are both complete.
  self_pointer_ = std::move(self);

  serving_url_loader_receiver_.Bind(std::move(receiver));
  serving_url_loader_receiver_.set_disconnect_handler(base::BindOnce(
      &PrefetchStreamingURLLoader::OnServingURLLoaderMojoDisconnect,
      weak_ptr_factory_.GetWeakPtr()));
  serving_url_loader_client_.Bind(std::move(client));

  // Serve the prefetched response by directly passing the |body_| mojo pipe.
  // All data that has already been received will be buffered in the pipe, and
  // all other data will be streamed as it is received.
  DCHECK(head_);
  DCHECK(body_);
  serving_url_loader_client_->OnReceiveResponse(
      head_->Clone(), std::move(body_), absl::nullopt);

  RunEventQueue();
}

void PrefetchStreamingURLLoader::RunEventQueue() {
  DCHECK(serving_url_loader_client_);
  for (auto& event : event_queue_) {
    std::move(event).Run();
  }
  event_queue_.clear();
}

void PrefetchStreamingURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  // Redirects aren't supported by prefetch, and therefore are never served.
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
