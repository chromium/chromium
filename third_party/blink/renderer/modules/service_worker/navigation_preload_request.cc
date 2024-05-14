// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/navigation_preload_request.h"

#include <utility>

#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom-blink.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"

namespace blink {

// static
std::unique_ptr<WebNavigationPreloadRequest>
WebNavigationPreloadRequest::Create(
    WebServiceWorkerContextClient* owner,
    int fetch_event_id,
    const WebURL& url,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        preload_url_loader_client_receiver) {
  return std::make_unique<NavigationPreloadRequest>(
      owner, fetch_event_id, url,
      std::move(preload_url_loader_client_receiver));
}

NavigationPreloadRequest::NavigationPreloadRequest(
    WebServiceWorkerContextClient* owner,
    int fetch_event_id,
    const WebURL& url,
    mojo::PendingReceiver<network::mojom::URLLoaderClient>
        preload_url_loader_client_receiver)
    : owner_(owner),
      fetch_event_id_(fetch_event_id),
      url_(url),
      receiver_(this, std::move(preload_url_loader_client_receiver)) {}

NavigationPreloadRequest::~NavigationPreloadRequest() = default;

void NavigationPreloadRequest::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {}

void NavigationPreloadRequest::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK(!response_);
  response_ = std::make_unique<WebURLResponse>();
  // TODO(horo): Set report_security_info to true when DevTools is attached.
  const bool report_security_info = false;
  *response_ = WebURLResponse::Create(
      url_, *response_head, report_security_info, -1 /* request_id */);
  body_ = std::move(body);
  MaybeReportResponseToOwner();
}

void NavigationPreloadRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK(!response_);
  DCHECK(net::HttpResponseHeaders::IsRedirectResponseCode(
      response_head->headers->response_code()));

  response_ = std::make_unique<WebURLResponse>();
  *response_ = WebURLResponse::Create(url_, *response_head,
                                      false /* report_security_info */,
                                      -1 /* request_id */);
  owner_->OnNavigationPreloadResponse(fetch_event_id_, std::move(response_),
                                      mojo::ScopedDataPipeConsumerHandle());
  // This will delete |this|.
  owner_->OnNavigationPreloadComplete(
      fetch_event_id_, response_head->response_start,
      response_head->encoded_data_length, 0 /* encoded_body_length */,
      0 /* decoded_body_length */);
}

void NavigationPreloadRequest::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTREACHED_IN_MIGRATION();
}

void NavigationPreloadRequest::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kNavigationPreloadRequest);
}

void NavigationPreloadRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK) {
    WebString message;
    WebServiceWorkerError::Mode error_mode = WebServiceWorkerError::Mode::kNone;
    if (status.error_code == net::ERR_ABORTED) {
      message =
          "The service worker navigation preload request was cancelled "
          "before 'preloadResponse' settled. If you intend to use "
          "'preloadResponse', use waitUntil() or respondWith() to wait for "
          "the promise to settle.";
      error_mode = WebServiceWorkerError::Mode::kShownInConsole;
    } else {
      message =
          "The service worker navigation preload request failed due to a "
          "network error. This may have been an actual network error, or "
          "caused by the browser simulating offline to see if the page works "
          "offline: see https://w3c.github.io/manifest/#installability-signals";
    }

    // This will delete |this|.
    ReportErrorToOwner(message, error_mode);
    return;
  }

  if (response_) {
    // When the response body from the server is empty, OnComplete() is called
    // without OnStartLoadingResponseBody().
    DCHECK(!body_.is_valid());
    owner_->OnNavigationPreloadResponse(fetch_event_id_, std::move(response_),
                                        mojo::ScopedDataPipeConsumerHandle());
  }
  // This will delete |this|.
  owner_->OnNavigationPreloadComplete(
      fetch_event_id_, status.completion_time, status.encoded_data_length,
      status.encoded_body_length, status.decoded_body_length);
}

void NavigationPreloadRequest::MaybeReportResponseToOwner() {
  if (!response_ || !body_.is_valid())
    return;
  owner_->OnNavigationPreloadResponse(fetch_event_id_, std::move(response_),
                                      std::move(body_));
}

void NavigationPreloadRequest::ReportErrorToOwner(
    const WebString& message,
    WebServiceWorkerError::Mode error_mode) {
  // This will delete |this|.
  owner_->OnNavigationPreloadError(
      fetch_event_id_,
      std::make_unique<WebServiceWorkerError>(
          mojom::blink::ServiceWorkerErrorType::kNetwork, message, error_mode));
}

}  // namespace blink
