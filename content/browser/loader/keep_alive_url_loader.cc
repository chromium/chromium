// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader.h"

#include "base/functional/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "third_party/blink/public/common/features.h"

namespace content {

KeepAliveURLLoader::KeepAliveURLLoader(
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& resource_request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> network_loader_factory,
    base::PassKey<KeepAliveURLLoaderService>)
    : request_id_(request_id),
      forwarding_client_(std::move(forwarding_client)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(network_loader_factory);
  DCHECK(!resource_request.trusted_params);
  TRACE_EVENT2("loading", "KeepAliveURLLoader::KeepAliveURLLoader",
               "request_id", request_id_, "url", resource_request.url);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("loading", "KeepAliveURLLoader",
                                    request_id_, "url", resource_request.url);

  // Asks the network service to create a URL loader with passed in params.
  network_loader_factory->CreateLoaderAndStart(
      loader_.BindNewPipeAndPassReceiver(), request_id, options,
      resource_request, loader_receiver_.BindNewPipeAndPassRemote(),
      traffic_annotation);
  loader_receiver_.set_disconnect_handler(base::BindOnce(
      &KeepAliveURLLoader::OnNetworkConnectionError, base::Unretained(this)));
  forwarding_client_.set_disconnect_handler(base::BindOnce(
      &KeepAliveURLLoader::OnRendererConnectionError, base::Unretained(this)));
}

KeepAliveURLLoader::~KeepAliveURLLoader() {
  TRACE_EVENT1("loading", "KeepAliveURLLoader::~KeepAliveURLLoader",
               "request_id", request_id_);
  TRACE_EVENT_NESTABLE_ASYNC_END0("loading", "KeepAliveURLLoader", request_id_);
}

void KeepAliveURLLoader::set_on_delete_callback(
    OnDeleteCallback on_delete_callback) {
  on_delete_callback_ = std::move(on_delete_callback);
}

void KeepAliveURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(loader_);
  TRACE_EVENT2("loading", "KeepAliveURLLoader::FollowRedirect", "request_id",
               request_id_, "url", new_url);

  // Forwards the action to `loader_` in the network service.
  loader_->FollowRedirect(removed_headers, modified_headers,
                          modified_cors_exempt_headers, new_url);
}

void KeepAliveURLLoader::SetPriority(net::RequestPriority priority,
                                     int intra_priority_value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(loader_);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::SetPriority", "request_id",
               request_id_);

  // Forwards the action to `loader_` in the network service.
  loader_->SetPriority(priority, intra_priority_value);
}

void KeepAliveURLLoader::PauseReadingBodyFromNet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(loader_);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::FollowRedirect", "request_id",
               request_id_);

  // Forwards the action to `loader_` in the network service.
  loader_->PauseReadingBodyFromNet();
}

void KeepAliveURLLoader::ResumeReadingBodyFromNet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(loader_);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::ResumeReadingBodyFromNet",
               "request_id", request_id_);

  // Forwards the action to `loader_` in the network service.
  loader_->ResumeReadingBodyFromNet();
}

void KeepAliveURLLoader::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::OnReceiveEarlyHints",
               "request_id", request_id_);

  if (forwarding_client_) {
    // The renderer is alive, forwards the action.
    forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
    return;
  }

  // TODO(crbug.com/1356128): Handle in browser process.
}

void KeepAliveURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response,
    mojo::ScopedDataPipeConsumerHandle body,
    absl::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::OnReceiveResponse", "request_id",
               request_id_);

  has_received_response_ = true;
  // TODO(crbug.com/1424731): The renderer might exit before `OnReceiveRedirect`
  // or `OnReceiveResponse` is called, or during their execution. In such case,
  // `forwarding_client_` can't finish response handling. Figure out a way to
  // negotiate shutdown timing via RenderFrameHostImpl::OnUnloadAck() and
  // invalidate `forwarding_client_`.
  if (forwarding_client_) {
    // The renderer is alive, forwards the action.
    // The receiver may fail to finish reading `response`, so response caching
    // is not guaranteed.
    forwarding_client_->OnReceiveResponse(std::move(response), std::move(body),
                                          std::move(cached_metadata));
    // TODO(crbug.com/1422645): Ensure that attributionsrc response handling is
    // migrated to browser process.
    return;
  }

  // No need to wait for `OnComplete()`.
  // This loader should be deleted immediately to avoid hanged requests taking
  // up resources.
  DeleteSelf();
  // DO NOT touch any members after this line. `this` is already deleted.
}

void KeepAliveURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::OnReceiveRedirect", "request_id",
               request_id_);

  // TODO(crbug.com/1424731): The renderer might exit before `OnReceiveRedirect`
  // or `OnReceiveResponse` is called, or during their execution. In such case,
  // `forwarding_client_` can't finish response handling. Figure out a way to
  // negotiate shutdown timing via RenderFrameHostImpl::OnUnloadAck() and
  // invalidate `forwarding_client_`.
  if (forwarding_client_) {
    // The renderer is alive, forwards the action.
    // Redirects must be handled by the renderer so that it know what URL the
    // response come from when parsing responses.
    forwarding_client_->OnReceiveRedirect(redirect_info, std::move(head));
    return;
  }

  // TODO(crbug.com/1356128): Replicates all existing behaviors from all of
  // `blink::URLLoaderThrottles`.
  // TODO(crbug.com/1356128): Run security checks, including CSP, mixed-content,
  // and SafeBrowsing.
  // TODO(crbug.com/1356128): Ask the network service to follow the redirect.
}

void KeepAliveURLLoader::OnUploadProgress(int64_t current_position,
                                          int64_t total_size,
                                          base::OnceCallback<void()> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::OnUploadProgress", "request_id",
               request_id_);

  if (forwarding_client_) {
    // The renderer is alive, forwards the action.
    forwarding_client_->OnUploadProgress(current_position, total_size,
                                         std::move(callback));
    return;
  }

  // TODO(crbug.com/1356128): Handle in the browser process.
}

void KeepAliveURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::OnTransferSizeUpdated",
               "request_id", request_id_);

  if (forwarding_client_) {
    // The renderer is alive, forwards the action.
    forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
    return;
  }

  // TODO(crbug.com/1356128): Handle in the browser process.
}

void KeepAliveURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& completion_status) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::OnComplete", "request_id",
               request_id_);

  if (forwarding_client_) {
    // The renderer is alive, forwards the action.
    forwarding_client_->OnComplete(completion_status);
  }

  DeleteSelf();
  // DO NOT touch any members after this line. `this` is already deleted.
}

void KeepAliveURLLoader::OnNetworkConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::OnNetworkConnectionError",
               "request_id", request_id_);

  // The network loader has an error; we should let the client know it's
  // closed by dropping this, which will in turn make this loader destroyed.
  forwarding_client_.reset();
}

void KeepAliveURLLoader::OnRendererConnectionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TRACE_EVENT1("loading", "KeepAliveURLLoader::OnRendererConnectionError",
               "request_id", request_id_);

  if (has_received_response_) {
    // No need to wait for `OnComplete()`.
    DeleteSelf();
    // DO NOT touch any members after this line. `this` is already deleted.
    return;
  }
  // Otherwise, let this loader continue to handle responses.
  forwarding_client_.reset();
  // TODO(crbug.com/1424731): When we reach here while the renderer is
  // processing a redirect, we should take over the redirect handling in the
  // browser process. See TODOs in `OnReceiveRedirect()`.
}

void KeepAliveURLLoader::DeleteSelf() {
  CHECK(on_delete_callback_);
  std::move(on_delete_callback_).Run();
}

}  // namespace content
