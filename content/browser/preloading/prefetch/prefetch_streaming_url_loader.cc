// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "net/cookies/cookie_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

PrefetchStreamingURLLoader::PrefetchStreamingURLLoader(
    OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
    OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback,
    OnPrefetchRedirectCallback on_prefetch_redirect_callback,
    base::OnceClosure on_determined_head_callback)
    : on_prefetch_response_started_callback_(
          std::move(on_prefetch_response_started_callback)),
      on_prefetch_response_completed_callback_(
          std::move(on_prefetch_response_completed_callback)),
      on_prefetch_redirect_callback_(std::move(on_prefetch_redirect_callback)),
      on_determined_head_callback_(std::move(on_determined_head_callback)) {}

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
  // here.
  network::ResourceRequest new_request(request);
  if (!new_request.trusted_params) {
    new_request.trusted_params.emplace();
  }

  // Request cookies will be included with the response.
  // They must be removed before forwarding to any untrusted client.
  // This happens in `PrefetchResponseReader::HandleRedirect` and
  // `PrefetchResponseReader::OnReceiveResponse`.
  new_request.trusted_params->include_request_cookies_with_response = true;

  // `is_outermost_main_frame` is true here because the prefetched result is
  // served only for outermost main frames.
  url_loader_factory->CreateLoaderAndStart(
      prefetch_url_loader_.BindNewPipeAndPassReceiver(), /*request_id=*/0,
      NavigationURLLoader::GetURLLoaderOptions(
          /*is_outermost_main_frame=*/true),
      new_request,
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
base::WeakPtr<PrefetchStreamingURLLoader>
PrefetchStreamingURLLoader::CreateAndStart(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const network::ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::TimeDelta timeout_duration,
    OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
    OnPrefetchResponseCompletedCallback on_prefetch_response_completed_callback,
    OnPrefetchRedirectCallback on_prefetch_redirect_callback,
    base::OnceClosure on_determined_head_callback,
    base::WeakPtr<PrefetchResponseReader> response_reader) {
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          std::move(on_prefetch_response_started_callback),
          std::move(on_prefetch_response_completed_callback),
          std::move(on_prefetch_redirect_callback),
          std::move(on_determined_head_callback));

  streaming_loader->SetResponseReader(std::move(response_reader));

  streaming_loader->Start(url_loader_factory, request,
                          network_traffic_annotation,
                          std::move(timeout_duration));

  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      streaming_loader->GetWeakPtr();
  weak_streaming_loader->self_pointer_ = std::move(streaming_loader);

  return weak_streaming_loader;
}

void PrefetchStreamingURLLoader::SetResponseReader(
    base::WeakPtr<PrefetchResponseReader> response_reader) {
  response_reader_ = std::move(response_reader);
  if (response_reader_) {
    response_reader_->SetStreamingURLLoader(GetWeakPtr());
  }
}

void PrefetchStreamingURLLoader::CancelIfNotServing() {
  if (used_for_serving_) {
    return;
  }
  DisconnectPrefetchURLLoaderMojo();
}

void PrefetchStreamingURLLoader::DisconnectPrefetchURLLoaderMojo() {
  prefetch_url_loader_.reset();
  prefetch_url_loader_client_receiver_.reset();
  timeout_timer_.AbandonAndStop();

  if (!self_pointer_) {
    return;
  }

  if (on_deletion_scheduled_for_tests_) {
    std::move(on_deletion_scheduled_for_tests_).Run();
  }

  // To avoid UAF bugs, post a separate task to delete this object, because this
  // can be called in one of PrefetchStreamingURLLoader callbacks.
  base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
      FROM_HERE, std::move(self_pointer_));
}

bool PrefetchStreamingURLLoader::IsDeletionScheduledForCHECK() const {
  return !self_pointer_;
}

void PrefetchStreamingURLLoader::SetOnDeletionScheduledForTests(
    base::OnceClosure on_deletion_scheduled_for_tests) {
  on_deletion_scheduled_for_tests_ = std::move(on_deletion_scheduled_for_tests);
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
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  // Cached metadata is not supported for prefetch.
  cached_metadata.reset();

  head->was_in_prefetch_cache = true;

  // Checks head to determine if the prefetch can be served.
  CHECK(on_prefetch_response_started_callback_);
  std::optional<PrefetchErrorOnResponseReceived> error =
      std::move(on_prefetch_response_started_callback_).Run(head.get());

  // `head` and `body` are discarded if `response_reader_` is `nullptr`, because
  // it means the `PrefetchResponseReader` is deleted and thus we no longer
  // serve the prefetched result.
  if (response_reader_) {
    response_reader_->OnReceiveResponse(std::move(error), std::move(head),
                                        std::move(body));
  }

  if (on_determined_head_callback_) {
    std::move(on_determined_head_callback_).Run();
  }
}

void PrefetchStreamingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  CHECK(on_prefetch_redirect_callback_);
  on_prefetch_redirect_callback_.Run(redirect_info, std::move(redirect_head));
}

void PrefetchStreamingURLLoader::HandleRedirect(
    PrefetchRedirectStatus redirect_status,
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  CHECK(redirect_head);

  // If the prefetch_url_loader_ is no longer connected, mark this as failed.
  if (!prefetch_url_loader_) {
    redirect_status = PrefetchRedirectStatus::kFail;
  }

  switch (redirect_status) {
    case PrefetchRedirectStatus::kFollow:
      CHECK(prefetch_url_loader_);
      prefetch_url_loader_->FollowRedirect(
          /*removed_headers=*/std::vector<std::string>(),
          /*modified_headers=*/net::HttpRequestHeaders(),
          /*modified_cors_exempt_headers=*/net::HttpRequestHeaders(),
          /*new_url=*/std::nullopt);
      break;
    case PrefetchRedirectStatus::kSwitchNetworkContext:
      // The redirect requires a switch in network context, so the redirect will
      // be followed using a separate PrefetchStreamingURLLoader, and this url
      // loader will stop its request.
      DisconnectPrefetchURLLoaderMojo();
      break;
    case PrefetchRedirectStatus::kFail:
      DisconnectPrefetchURLLoaderMojo();
      if (on_determined_head_callback_) {
        std::move(on_determined_head_callback_).Run();
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
  NOTREACHED_IN_MIGRATION();
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

  if (response_reader_) {
    response_reader_->OnComplete(completion_status);
  }

  if (completion_status.error_code != net::OK) {
    // Notify a failure if the callback is not consumed yet.
    if (on_determined_head_callback_) {
      std::move(on_determined_head_callback_).Run();
    }
  }

  std::move(on_prefetch_response_completed_callback_).Run(completion_status);
}

void PrefetchStreamingURLLoader::OnStartServing() {
  // Once the prefetch is served, stop the timeout timer.
  timeout_timer_.AbandonAndStop();

  used_for_serving_ = true;
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

}  // namespace content
