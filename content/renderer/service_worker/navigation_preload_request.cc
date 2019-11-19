// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/navigation_preload_request.h"

#include <utility>

#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/service_worker/service_worker_context_client.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_error_type.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"

namespace content {

NavigationPreloadRequest::NavigationPreloadRequest(
    ServiceWorkerContextClient* owner,
    int fetch_event_id,
    const GURL& url,
    blink::mojom::FetchEventPreloadHandlePtr preload_handle)
    : owner_(owner),
      fetch_event_id_(fetch_event_id),
      url_(url),
      url_loader_(std::move(preload_handle->url_loader)),
      receiver_(this, std::move(preload_handle->url_loader_client_receiver)) {}

NavigationPreloadRequest::~NavigationPreloadRequest() = default;

void NavigationPreloadRequest::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK(!response_);
  response_ = std::make_unique<blink::WebURLResponse>();
  // TODO(horo): Set report_security_info to true when DevTools is attached.
  const bool report_security_info = false;
  WebURLLoaderImpl::PopulateURLResponse(url_, *response_head, response_.get(),
                                        report_security_info,
                                        -1 /* request_id */);
  MaybeReportResponseToOwner();
}

void NavigationPreloadRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  DCHECK(!response_);
  DCHECK(net::HttpResponseHeaders::IsRedirectResponseCode(
      response_head->headers->response_code()));

  response_ = std::make_unique<blink::WebURLResponse>();
  WebURLLoaderImpl::PopulateURLResponse(url_, *response_head, response_.get(),
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
  NOTREACHED();
}

void NavigationPreloadRequest::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {}

void NavigationPreloadRequest::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {}

void NavigationPreloadRequest::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(!body_.is_valid());
  body_ = std::move(body);
  MaybeReportResponseToOwner();
}

void NavigationPreloadRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK) {
    std::string message;
    std::string unsanitized_message;
    if (status.error_code == net::ERR_ABORTED) {
      message =
          "The service worker navigation preload request was cancelled "
          "before 'preloadResponse' settled. If you intend to use "
          "'preloadResponse', use waitUntil() or respondWith() to wait for "
          "the promise to settle.";
    } else {
      message =
          "The service worker navigation preload request failed with a "
          "network error.";
      unsanitized_message =
          "The service worker navigation preload request failed with network "
          "error: " +
          net::ErrorToString(status.error_code) + ".";
    }

    // This will delete |this|.
    ReportErrorToOwner(message, unsanitized_message);
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
    const std::string& message,
    const std::string& unsanitized_message) {
  // This will delete |this|.
  owner_->OnNavigationPreloadError(
      fetch_event_id_, std::make_unique<blink::WebServiceWorkerError>(
                           blink::mojom::ServiceWorkerErrorType::kNetwork,
                           blink::WebString::FromUTF8(message),
                           blink::WebString::FromUTF8(unsanitized_message)));
}

}  // namespace content
