// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_streaming_url_loader.h"

#include "base/check_is_test.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/service_worker/service_worker_main_resource_loader_interceptor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace content {

PrefetchStreamingURLLoader::PrefetchStreamingURLLoader(
    OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
    OnPrefetchRedirectCallback on_prefetch_redirect_callback,
    OnServiceWorkerStateDeterminedCallback
        on_service_worker_state_determined_callback)
    : on_prefetch_response_started_callback_(
          std::move(on_prefetch_response_started_callback)),
      on_prefetch_redirect_callback_(std::move(on_prefetch_redirect_callback)),
      on_service_worker_state_determined_callback_(
          std::move(on_service_worker_state_determined_callback)) {}

void PrefetchStreamingURLLoader::Start(
    PrefetchServiceWorkerState final_service_worker_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const network::ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::TimeDelta timeout_duration) {
  std::move(on_service_worker_state_determined_callback_)
      .Run(final_service_worker_state);

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
  if (base::FeatureList::IsEnabled(features::kPrefetchGracefulNotification)) {
    // We call `DisconnectPrefetchURLLoaderMojo()` in `OnComplete()`, so
    // disconnection reaching this handler is always unexpected disconnection
    // before `OnComplete()` and should be considered as a failure.
    prefetch_url_loader_client_receiver_.set_disconnect_handler(base::BindOnce(
        &PrefetchStreamingURLLoader::OnComplete, weak_ptr_factory_.GetWeakPtr(),
        network::URLLoaderCompletionStatus(net::ERR_ABORTED)));
  } else {
    prefetch_url_loader_client_receiver_.set_disconnect_handler(base::BindOnce(
        &PrefetchStreamingURLLoader::DisconnectPrefetchURLLoaderMojo,
        weak_ptr_factory_.GetWeakPtr()));
  }

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
    scoped_refptr<network::SharedURLLoaderFactory> network_url_loader_factory,
    const network::ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::TimeDelta timeout_duration,
    OnPrefetchResponseStartedCallback on_prefetch_response_started_callback,
    OnPrefetchRedirectCallback on_prefetch_redirect_callback,
    base::WeakPtr<PrefetchResponseReader> response_reader,
    PrefetchServiceWorkerState initial_service_worker_state,
    BrowserContext* browser_context_for_service_worker,
    OnServiceWorkerStateDeterminedCallback
        on_service_worker_state_determined_callback) {
  TRACE_EVENT("loading", "PrefetchStreamingURLLoader::CreateAndStart");
  std::unique_ptr<PrefetchStreamingURLLoader> streaming_loader =
      std::make_unique<PrefetchStreamingURLLoader>(
          std::move(on_prefetch_response_started_callback),
          std::move(on_prefetch_redirect_callback),
          std::move(on_service_worker_state_determined_callback));

  streaming_loader->SetResponseReader(std::move(response_reader));

  base::WeakPtr<PrefetchStreamingURLLoader> weak_streaming_loader =
      streaming_loader->GetWeakPtr();
  weak_streaming_loader->self_pointer_ = std::move(streaming_loader);

  switch (initial_service_worker_state) {
    case PrefetchServiceWorkerState::kAllowed:
      weak_streaming_loader->StartServiceWorkerInterceptor(
          browser_context_for_service_worker,
          std::move(network_url_loader_factory), request,
          network_traffic_annotation, std::move(timeout_duration));
      break;
    case PrefetchServiceWorkerState::kDisallowed:
      weak_streaming_loader->Start(PrefetchServiceWorkerState::kDisallowed,
                                   std::move(network_url_loader_factory),
                                   request, network_traffic_annotation,
                                   std::move(timeout_duration));
      break;
    case PrefetchServiceWorkerState::kControlled:
      NOTREACHED();
  }

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
  // If this is going to be deleted while waiting for redirect handling from
  // `PrefetchService`, treat it as a redirect failure.
  if (is_waiting_handle_redirect_from_prefetch_service_) {
    is_waiting_handle_redirect_from_prefetch_service_ = false;
    if (response_reader_) {
      response_reader_->HandleRedirect(PrefetchRedirectStatus::kFail, {}, {});
    }
  }
  prefetch_url_loader_.reset();
  prefetch_url_loader_client_receiver_.reset();
  timeout_timer_.Stop();

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
                                        std::move(body),
                                        std::move(service_worker_handle_));
  }
}

void PrefetchStreamingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  is_waiting_handle_redirect_from_prefetch_service_ = true;
  CHECK(on_prefetch_redirect_callback_);
  on_prefetch_redirect_callback_.Run(redirect_info, std::move(redirect_head));
}

void PrefetchStreamingURLLoader::HandleRedirect(
    PrefetchRedirectStatus redirect_status,
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr redirect_head) {
  if (!is_waiting_handle_redirect_from_prefetch_service_) {
    // The redirect should have already been handled.
    return;
  }
  is_waiting_handle_redirect_from_prefetch_service_ = false;
  CHECK(redirect_head);
  if (response_reader_) {
    response_reader_->HandleRedirect(redirect_status, redirect_info,
                                     std::move(redirect_head));
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
  is_waiting_handle_redirect_from_prefetch_service_ = false;

  if (response_reader_) {
    response_reader_->OnComplete(completion_status);
  }

  DisconnectPrefetchURLLoaderMojo();
}

void PrefetchStreamingURLLoader::OnStartServing() {
  // Once the prefetch is served, stop the timeout timer.
  timeout_timer_.Stop();

  if (base::FeatureList::IsEnabled(
          features::kPrefetchBumpNetworkPriorityAfterBeingServed)) {
    SetPriority(net::RequestPriority::HIGHEST, /*intra_priority_value=*/0);
  }

  used_for_serving_ = true;
}

void PrefetchStreamingURLLoader::SetPriority(net::RequestPriority priority,
                                             int32_t intra_priority_value) {
  if (prefetch_url_loader_) {
    prefetch_url_loader_->SetPriority(priority, intra_priority_value);
  }
}

void PrefetchStreamingURLLoader::StartServiceWorkerInterceptor(
    BrowserContext* browser_context,
    scoped_refptr<network::SharedURLLoaderFactory> network_url_loader_factory,
    const network::ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::TimeDelta timeout_duration) {
  auto callback = base::BindOnce(
      &PrefetchStreamingURLLoader::ServiceWorkerInterceptorLoaderCallback,
      GetWeakPtr(), network_url_loader_factory, request,
      network_traffic_annotation, std::move(timeout_duration));

  if (!browser_context) {
    // In tests, `browser_context` can be null. Emulate as if there are no
    // service workers without going through the interceptor.
    CHECK_IS_TEST();
    std::move(callback).Run(std::nullopt);
    return;
  }

  // TODO(https://crbug.com/40947546): Set this FetchEvent's Client ID.
  std::string fetch_event_client_id;

  // For navigations, FrameTreeNode's `pending_frame_policy().sandbox_flags` is
  // checked, to prevent using ServiceWorker for sandboxed iframes. We can't
  // check it here (because prefetches don't have FrameTreeNode), and probably
  // don't have to (because prefetches are always top-level and doesn't support
  // iframes in the first place).

  ServiceWorkerContextWrapper* service_worker_context =
      static_cast<ServiceWorkerContextWrapper*>(
          browser_context->GetDefaultStoragePartition()
              ->GetServiceWorkerContext());
  service_worker_handle_ = std::make_unique<ServiceWorkerMainResourceHandle>(
      service_worker_context, base::DoNothing(),
      std::move(fetch_event_client_id));

  interceptor_ = ServiceWorkerMainResourceLoaderInterceptor::CreateForPrefetch(
      request, service_worker_handle_->AsWeakPtr(), network_url_loader_factory);
  if (!interceptor_) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  interceptor_->MaybeCreateLoader(
      request, browser_context, std::move(callback),
      base::BindOnce(
          [](scoped_refptr<network::SharedURLLoaderFactory>
                 network_url_loader_factory,
             ResponseHeadUpdateParams) {
            // TODO(https://crbug.com/40947546): Handle
            // `ResponseHeadUpdateParams` if needed.
            return static_cast<network::mojom::URLLoaderFactory*>(
                network_url_loader_factory.get());
          },
          network_url_loader_factory));
}

void PrefetchStreamingURLLoader::ServiceWorkerInterceptorLoaderCallback(
    scoped_refptr<network::SharedURLLoaderFactory> network_url_loader_factory,
    const network::ResourceRequest& request,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    base::TimeDelta timeout_duration,
    std::optional<NavigationLoaderInterceptor::Result> interceptor_result) {
  if (!interceptor_result) {
    // Controlling ServiceWorker is not found.

    // Discard the `ServiceWorkerClient`, and fallback to non-SW-controlled
    // prefetching.
    service_worker_handle_.reset();
    interceptor_.reset();

    Start(PrefetchServiceWorkerState::kDisallowed,
          std::move(network_url_loader_factory), request,
          network_traffic_annotation, std::move(timeout_duration));
    return;
  }

  // `interceptor_result->single_request_factory` can be still null here, e.g.
  // when there is a ServiceWorker controller with no fetch handler. In such
  // cases, `network_url_loader_factory` is used for prefetching, while still
  // being considered as ServiceWorker-controlled.
  Start(PrefetchServiceWorkerState::kControlled,
        interceptor_result->single_request_factory
            ? std::move(interceptor_result->single_request_factory)
            : std::move(network_url_loader_factory),
        request, network_traffic_annotation, std::move(timeout_duration));
}

}  // namespace content
