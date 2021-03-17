// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_url_loader_request.h"

#include "base/bind.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_update_url_fetcher.h"
#include "content/browser/storage_partition_impl.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_response_info.h"

namespace content {

namespace {

constexpr net::NetworkTrafficAnnotationTag kAppCacheTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("appcache_update_job", R"(
      semantics {
        sender: "HTML5 AppCache System"
        description:
          "Web pages can include a link to a manifest file which lists "
          "resources to be cached for offline access. The AppCache system"
          "retrieves those resources in the background."
        trigger:
          "User visits a web page containing a <html manifest=manifestUrl> "
          "tag, or navigates to a document retrieved from an existing appcache "
          "and some resource should be updated."
        data: "None"
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting:
          "Users can control this feature via the 'Cookies' setting under "
          "'Privacy, Content settings'. If cookies are disabled for a single "
          "site, appcaches are disabled for the site only. If they are totally "
          "disabled, all appcache requests will be stopped."
        chrome_policy {
            DefaultCookiesSetting {
              DefaultCookiesSetting: 2
            }
          }
      })");

const char kAppCacheAllowed[] = "X-AppCache-Allowed";
}

AppCacheUpdateJob::UpdateURLLoaderRequest::~UpdateURLLoaderRequest() = default;

void AppCacheUpdateJob::UpdateURLLoaderRequest::Start() {
  // If we are in tests mode, we don't need to issue network requests.
  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  // The partition has shutdown, return without making the request.
  if (!partition_)
    return;
  partition_->GetURLLoaderFactoryForBrowserProcessWithCORBEnabled()
      ->CreateLoaderAndStart(
          url_loader_.BindNewPipeAndPassReceiver(), -1,
          network::mojom::kURLLoadOptionSendSSLInfoWithResponse, request_,
          client_receiver_.BindNewPipeAndPassRemote(),
          net::MutableNetworkTrafficAnnotationTag(kAppCacheTrafficAnnotation));
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
  return response_->mime_type;
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetSiteForCookies(
    const GURL& site_for_cookies) {
  request_.site_for_cookies = net::SiteForCookies::FromUrl(site_for_cookies);
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::SetInitiator(
    const base::Optional<url::Origin>& initiator) {
  request_.request_initiator = initiator;
}

net::HttpResponseHeaders*
AppCacheUpdateJob::UpdateURLLoaderRequest::GetResponseHeaders() const {
  if (!response_)
    return nullptr;
  return response_->headers.get();
}

int AppCacheUpdateJob::UpdateURLLoaderRequest::GetResponseCode() const {
  if (response_->headers)
    return response_->headers->response_code();
  return 0;
}

std::string
AppCacheUpdateJob::UpdateURLLoaderRequest::GetAppCacheAllowedHeader() const {
  std::string string_value;
  if (!response_->headers || !response_->headers->EnumerateHeader(
                                 nullptr, kAppCacheAllowed, &string_value)) {
    return "";
  }
  return string_value;
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
  client_receiver_.reset();
  url_loader_.reset();
  handle_watcher_.Cancel();
  handle_.reset();
  response_ = nullptr;
  http_response_info_.reset(nullptr);
  read_requested_ = false;
  return 0;
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  response_ = std::move(response_head);

  // TODO(ananta/michaeln)
  // Populate other fields in the HttpResponseInfo class. It would be good to
  // have a helper function which populates the HttpResponseInfo structure from
  // the URLResponseHead structure.
  http_response_info_ = std::make_unique<net::HttpResponseInfo>();
  if (response_->ssl_info.has_value())
    http_response_info_->ssl_info = *response_->ssl_info;
  http_response_info_->headers = response_->headers;
  http_response_info_->was_fetched_via_spdy = response_->was_fetched_via_spdy;
  http_response_info_->was_alpn_negotiated = response_->was_alpn_negotiated;
  http_response_info_->alpn_negotiated_protocol =
      response_->alpn_negotiated_protocol;
  http_response_info_->connection_info = response_->connection_info;
  http_response_info_->remote_endpoint = response_->remote_endpoint;
  http_response_info_->request_time = response_->request_time;
  http_response_info_->response_time = response_->response_time;
  fetcher_->OnResponseStarted(net::OK);
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head) {
  response_ = std::move(response_head);
  fetcher_->OnReceivedRedirect(redirect_info);
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback ack_callback) {
  NOTIMPLEMENTED();
}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnReceiveCachedMetadata(
    mojo_base::BigBuffer data) {}

void AppCacheUpdateJob::UpdateURLLoaderRequest::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
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
    base::WeakPtr<StoragePartitionImpl> partition,
    const GURL& url,
    int buffer_size,
    URLFetcher* fetcher)
    : fetcher_(fetcher),
      partition_(std::move(partition)),
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
