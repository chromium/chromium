// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_url_loader_request.h"

#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_update_url_fetcher.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_context.h"

namespace content {

AppCacheUpdateJob::UpdateURLLoaderRequest::~UpdateURLLoaderRequest() {}

void AppCacheUpdateJob::UpdateURLLoaderRequest::Start() {
  // If we are in tests mode, we don't need to issue network requests.
  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  network::mojom::URLLoaderClientPtr client;
  client_binding_.Bind(mojo::MakeRequest(&client));

  loader_factory_getter_->GetNetworkFactory()->CreateLoaderAndStart(
      mojo::MakeRequest(&url_loader_), -1, -1,
      network::mojom::kURLLoadOptionNone, request_, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(GetTrafficAnnotation()));
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  request_.headers = headers;
}

GURL AppCacheUpdateJob::UpdateURLLoaderRequest::GetURL() const {
  return request_.url;
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetLoadFlags(int flags) {
  request_.load_flags = flags;
}

int AppCacheUpdateJob::UpdateURLLoaderRequest::GetLoadFlags() const {
  return request_.load_flags;
}

std::string AppCacheUpdateJob::UpdateURLLoaderRequest::GetMimeType() const {
  return response_.mime_type;
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetSiteForCookies(
    const GURL& site_for_cookies) {
  request_.site_for_cookies = site_for_cookies;
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetInitiator(
    const base::Optional<url::Origin>& initiator) {
  request_.request_initiator = initiator;
}

net::HttpResponseHeaders*
AppCacheUpdateJob::UpdateURLLoaderRequest::GetResponseHeaders() const {
  return response_.headers.get();
}

int AppCacheUpdateJob::UpdateURLLoaderRequest::GetResponseCode() const {
  if (response_.headers)
    return response_.headers->response_code();
  return 0;
}

const net::HttpResponseInfo&
AppCacheUpdateJob::UpdateURLLoaderRequest::GetResponseInfo() const {
  return *http_response_info_;
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::Read() {
  DCHECK(!read_requested_);

  read_requested_ = true;
  // Initiate a read from the pipe if we have not done so.
  MaybeStartReading();
}

int AppCacheUpdateJob::UpdateURLLoaderRequest::Cancel() {
  client_binding_.Close();
  url_loader_ = nullptr;
  handle_watcher_.Cancel();
  handle_.reset();
  response_ = network::ResourceResponseHead();
  http_response_info_.reset(nullptr);
  read_requested_ = false;
  return 0;
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveResponse(
    const network::ResourceResponseHead& response_head) {
  response_ = response_head;

  // TODO(ananta/michaeln)
  // Populate other fields in the HttpResponseInfo class. It would be good to
  // have a helper function which populates the HttpResponseInfo structure from
  // the ResourceResponseHead structure.
  http_response_info_.reset(new net::HttpResponseInfo());
  if (response_head.ssl_info.has_value())
    http_response_info_->ssl_info = *response_head.ssl_info;
  http_response_info_->headers = response_head.headers;
  http_response_info_->was_fetched_via_spdy =
      response_head.was_fetched_via_spdy;
  http_response_info_->was_alpn_negotiated = response_head.was_alpn_negotiated;
  http_response_info_->alpn_negotiated_protocol =
      response_head.alpn_negotiated_protocol;
  http_response_info_->connection_info = response_head.connection_info;
  http_response_info_->socket_address = response_head.socket_address;
  fetcher_->OnResponseStarted(net::OK);
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  response_ = response_head;
  fetcher_->OnReceivedRedirect(redirect_info);
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  handle_ = std::move(body);

  handle_watcher_.Watch(
      handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
      base::BindRepeating(
          &AppCacheUpdateJob::UpdateURLLoaderRequest::StartReading,
          base::Unretained(this)));

  // Initiate a read from the pipe if we have a pending Read() request.
  MaybeStartReading();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  response_status_ = status;
  // We inform the URLFetcher about a failure only here. For the success case
  // OnResponseCompleted() is invoked by URLFetcher::OnReadCompleted().
  if (status.error_code != net::OK)
    fetcher_->OnResponseCompleted(status.error_code);
}

AppCacheUpdateJob::UpdateURLLoaderRequest::UpdateURLLoaderRequest(
    URLLoaderFactoryGetter* loader_factory_getter,
    const GURL& url,
    int buffer_size,
    URLFetcher* fetcher)
    : fetcher_(fetcher),
      loader_factory_getter_(loader_factory_getter),
      client_binding_(this),
      buffer_size_(buffer_size),
      handle_watcher_(FROM_HERE,
                      mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                      base::SequencedTaskRunnerHandle::Get()),
      read_requested_(false) {
  request_.url = url;
  request_.method = "GET";
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::StartReading(
    MojoResult unused) {
  DCHECK(read_requested_);

  // Get the handle_ from a previous read operation if we have one.
  if (pending_read_) {
    DCHECK(pending_read_->IsComplete());
    handle_ = pending_read_->ReleaseHandle();
    pending_read_ = nullptr;
  }

  uint32_t available = 0;
  MojoResult result = network::MojoToNetPendingBuffer::BeginRead(
      &handle_, &pending_read_, &available);
  DCHECK_NE(result, MOJO_RESULT_BUSY);

  if (result == MOJO_RESULT_SHOULD_WAIT) {
    handle_watcher_.ArmOrNotify();
    return;
  }

  read_requested_ = false;

  if (result == MOJO_RESULT_FAILED_PRECONDITION) {
    DCHECK_EQ(response_status_.error_code, net::OK);
    fetcher_->OnReadCompleted(nullptr, 0);
    return;
  }

  if (result != MOJO_RESULT_OK) {
    fetcher_->OnResponseCompleted(net::ERR_FAILED);
    return;
  }

  int bytes_to_be_read = std::min<int>(buffer_size_, available);

  auto buffer = base::MakeRefCounted<network::MojoToNetIOBuffer>(
      pending_read_.get(), bytes_to_be_read);

  fetcher_->OnReadCompleted(buffer.get(), bytes_to_be_read);
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::MaybeStartReading() {
  if (!read_requested_)
    return;

  if (handle_watcher_.IsWatching())
    handle_watcher_.ArmOrNotify();
}

}  // namespace content
