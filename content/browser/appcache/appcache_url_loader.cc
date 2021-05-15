// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_url_loader.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/appcache/appcache_disk_cache_ops.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/browser/appcache/appcache_request.h"
#include "content/browser/appcache/appcache_request_handler.h"
#include "content/browser/appcache/appcache_response_info.h"
#include "content/browser/appcache/appcache_subresource_url_factory.h"
#include "content/browser/url_loader_factory_getter.h"
#include "net/base/ip_endpoint.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/net_adapters.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace content {

AppCacheURLLoader::~AppCacheURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (storage_.get())
    storage_->CancelDelegateCallbacks(this);
}

void AppCacheURLLoader::InitializeRangeRequestInfo(
    const net::HttpRequestHeaders& headers) {
  std::string value;
  std::vector<net::HttpByteRange> ranges;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &value) ||
      !net::HttpUtil::ParseRangeHeader(value, &ranges)) {
    return;
  }

  // If multiple ranges are requested, we play dumb and
  // return the entire response with 200 OK.
  if (ranges.size() == 1U)
    range_requested_ = ranges[0];
}

void AppCacheURLLoader::SetupRangeResponse() {
  DCHECK(is_range_request() && reader_.get() && IsDeliveringAppCacheResponse());
  int resource_size = static_cast<int>(info_->response_data_size());
  if (resource_size < 0 || !range_requested_.ComputeBounds(resource_size)) {
    range_requested_ = net::HttpByteRange();
    return;
  }

  DCHECK(range_requested_.IsValid());
  int offset = static_cast<int>(range_requested_.first_byte_position());
  int length = static_cast<int>(range_requested_.last_byte_position() -
                                range_requested_.first_byte_position() + 1);

  // Tell the reader about the range to read.
  reader_->SetReadRange(offset, length);

  // Make a copy of the full response headers and fix them up
  // for the range we'll be returning.
  range_response_info_ =
      std::make_unique<net::HttpResponseInfo>(info_->http_response_info());
  net::HttpResponseHeaders* headers = range_response_info_->headers.get();
  headers->UpdateWithNewRange(range_requested_, resource_size,
                              /*replace_status_line=*/true);
}

bool AppCacheURLLoader::IsStarted() const {
  return delivery_type_ != DeliveryType::kAwaitingDeliverCall &&
         delivery_type_ != DeliveryType::kNetwork;
}

void AppCacheURLLoader::DeliverAppCachedResponse(const GURL& manifest_url,
                                                 int64_t cache_id,
                                                 const AppCacheEntry& entry,
                                                 bool is_fallback) {
  if (!storage_.get() || !appcache_request_) {
    DeliverErrorResponse();
    return;
  }

  delivery_type_ = DeliveryType::kAppCached;

  // In tests we only care about the delivery_type_ state.
  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  load_timing_info_.request_start_time = base::Time::Now();
  load_timing_info_.request_start = base::TimeTicks::Now();

  manifest_url_ = manifest_url;
  cache_id_ = cache_id;
  entry_ = entry;
  is_fallback_ = is_fallback;

  if (is_fallback_ && loader_callback_)
    CallLoaderCallback({});

  InitializeRangeRequestInfo(appcache_request_->GetHeaders());
  storage_->LoadResponseInfo(manifest_url_, entry_.response_id(), this);
}

void AppCacheURLLoader::DeliverNetworkResponse() {
  delivery_type_ = DeliveryType::kNetwork;

  // In tests we only care about the delivery_type_ state.
  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  // We signal our caller with an empy callback that it needs to perform
  // the network load.
  DCHECK(loader_callback_);
  DCHECK(!receiver_.is_bound());
  std::move(loader_callback_).Run({});
  DeleteSoon();
}

void AppCacheURLLoader::DeliverErrorResponse() {
  delivery_type_ = DeliveryType::kError;

  // In tests we only care about the delivery_type_ state.
  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  if (loader_callback_) {
    CallLoaderCallback(base::BindOnce(&AppCacheURLLoader::NotifyCompleted,
                                      GetWeakPtr(), net::ERR_FAILED));
  } else {
    NotifyCompleted(net::ERR_FAILED);
  }
}

base::WeakPtr<AppCacheURLLoader> AppCacheURLLoader::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void AppCacheURLLoader::FollowRedirect(
    const std::vector<std::string>& modified_headers,
    const net::HttpRequestHeaders& removed_headers,
    const net::HttpRequestHeaders& removed_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  NOTREACHED() << "appcache never produces redirects";
}

void AppCacheURLLoader::SetPriority(net::RequestPriority priority,
                                    int32_t intra_priority_value) {}
void AppCacheURLLoader::PauseReadingBodyFromNet() {}
void AppCacheURLLoader::ResumeReadingBodyFromNet() {}

void AppCacheURLLoader::DeleteIfNeeded() {
  if (receiver_.is_bound() || is_deleting_soon_)
    return;
  delete this;
}

void AppCacheURLLoader::Start(
    base::OnceClosure continuation,
    const network::ResourceRequest& /* resource_request */,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  // TODO(crbug.com/876531): Figure out how AppCache interception should
  // interact with URLLoaderThrottles. It might be incorrect to ignore
  // |resource_request| here, since it's the current request after throttles.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  client_.Bind(std::move(client));
  receiver_.set_disconnect_handler(
      base::BindOnce(&AppCacheURLLoader::DeleteSoon, GetWeakPtr()));

  MojoResult result =
      mojo::CreateDataPipe(nullptr, response_body_stream_, consumer_handle_);
  if (result != MOJO_RESULT_OK) {
    NotifyCompleted(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }
  DCHECK(response_body_stream_.is_valid());
  DCHECK(consumer_handle_.is_valid());

  if (continuation)
    std::move(continuation).Run();
}

AppCacheURLLoader::AppCacheURLLoader(
    AppCacheRequest* appcache_request,
    AppCacheStorage* storage,
    AppCacheRequestHandler::AppCacheLoaderCallback loader_callback)
    : storage_(storage->GetWeakPtr()),
      start_time_tick_(base::TimeTicks::Now()),
      writable_handle_watcher_(FROM_HERE,
                               mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                               base::SequencedTaskRunnerHandle::Get()),
      loader_callback_(std::move(loader_callback)),
      appcache_request_(appcache_request->GetWeakPtr()) {}

void AppCacheURLLoader::CallLoaderCallback(base::OnceClosure continuation) {
  DCHECK(loader_callback_);
  DCHECK(!receiver_.is_bound());
  std::move(loader_callback_)
      .Run(base::BindOnce(&AppCacheURLLoader::Start, GetWeakPtr(),
                          std::move(continuation)));
}

void AppCacheURLLoader::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info,
    int64_t response_id) {
  DCHECK(IsDeliveringAppCacheResponse());

  if (!storage_.get()) {
    DeliverErrorResponse();
    return;
  }

  if (response_info) {
    if (loader_callback_) {
      CallLoaderCallback(
          base::BindOnce(&AppCacheURLLoader::ContinueOnResponseInfoLoaded,
                         GetWeakPtr(), base::WrapRefCounted(response_info)));
    } else {
      ContinueOnResponseInfoLoaded(response_info);
    }
    return;
  }

  // We failed to load the response headers from the disk cache.
  if (storage_->service()->storage() == storage_.get()) {
    // A resource that is expected to be in the appcache is missing.
    // If the 'check' fails, the corrupt appcache will be deleted.
    // See http://code.google.com/p/chromium/issues/detail?id=50657
    storage_->service()->CheckAppCacheResponse(manifest_url_, cache_id_,
                                               entry_.response_id());
  }
  cache_entry_not_found_ = true;

  // We fallback to the network unless this loader was falling back to the
  // appcache from the network which had already failed in some way.
  if (!is_fallback_)
    DeliverNetworkResponse();
  else
    DeliverErrorResponse();
}

void AppCacheURLLoader::ContinueOnResponseInfoLoaded(
    scoped_refptr<AppCacheResponseInfo> response_info) {
  info_ = response_info;
  reader_ = storage_->CreateResponseReader(manifest_url_, entry_.response_id());

  if (is_range_request())
    SetupRangeResponse();

  // TODO(ananta)
  // Move the asynchronous reading and mojo pipe handling code to a helper
  // class. That would also need a change to BlobURLLoader.

  // Wait for the data pipe to be ready to accept data.
  writable_handle_watcher_.Watch(
      response_body_stream_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      base::BindRepeating(&AppCacheURLLoader::OnResponseBodyStreamReady,
                          GetWeakPtr()));

  SendResponseInfo();
  ReadMore();
}

void AppCacheURLLoader::OnReadComplete(int result) {
  if (result > 0) {
    uint32_t bytes_written = static_cast<uint32_t>(result);
    response_body_stream_ = pending_write_->Complete(bytes_written);
    pending_write_ = nullptr;
    ReadMore();
    return;
  }

  writable_handle_watcher_.Cancel();
  pending_write_->Complete(0);
  pending_write_ = nullptr;
  NotifyCompleted(result);
}

void AppCacheURLLoader::OnResponseBodyStreamReady(MojoResult result) {
  // TODO(ananta)
  // Add proper error handling here.
  if (result != MOJO_RESULT_OK) {
    NotifyCompleted(net::ERR_FAILED);
    return;
  }
  ReadMore();
}

void AppCacheURLLoader::DeleteSoon() {
  if (storage_.get())
    storage_->CancelDelegateCallbacks(this);
  weak_factory_.InvalidateWeakPtrs();
  is_deleting_soon_ = true;
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void AppCacheURLLoader::SendResponseInfo() {
  // If this is null it means the response information was sent to the client.
  if (!consumer_handle_.is_valid())
    return;

  const net::HttpResponseInfo& http_info =
      is_range_request() ? *range_response_info_ : info_->http_response_info();
  auto response_head = network::mojom::URLResponseHead::New();
  response_head->headers = http_info.headers;
  response_head->appcache_id = cache_id_;
  response_head->appcache_manifest_url = manifest_url_;

  http_info.headers->GetMimeType(&response_head->mime_type);
  http_info.headers->GetCharset(&response_head->charset);

  // TODO(ananta)
  // Verify if the times sent here are correct.
  response_head->request_time = http_info.request_time;
  response_head->response_time = http_info.response_time;
  response_head->content_length =
      is_range_request() ? range_response_info_->headers->GetContentLength()
                         : info_->response_data_size();
  response_head->connection_info = http_info.connection_info;
  response_head->remote_endpoint = http_info.remote_endpoint;
  response_head->was_fetched_via_spdy = http_info.was_fetched_via_spdy;
  response_head->was_alpn_negotiated = http_info.was_alpn_negotiated;
  response_head->alpn_negotiated_protocol = http_info.alpn_negotiated_protocol;
  if (http_info.ssl_info.cert)
    response_head->ssl_info = http_info.ssl_info;
  response_head->load_timing = load_timing_info_;

  client_->OnReceiveResponse(std::move(response_head));
  client_->OnStartLoadingResponseBody(std::move(consumer_handle_));
}

void AppCacheURLLoader::ReadMore() {
  DCHECK(!pending_write_.get());

  uint32_t num_bytes;
  // TODO: we should use the abstractions in MojoAsyncResourceHandler.
  MojoResult result = network::NetToMojoPendingBuffer::BeginWrite(
      &response_body_stream_, &pending_write_, &num_bytes);
  if (result == MOJO_RESULT_SHOULD_WAIT) {
    // The pipe is full. We need to wait for it to have more space.
    writable_handle_watcher_.ArmOrNotify();
    return;
  }
  if (result != MOJO_RESULT_OK) {
    NotifyCompleted(net::ERR_FAILED);
    writable_handle_watcher_.Cancel();
    response_body_stream_.reset();
    return;
  }

  CHECK_GT(static_cast<uint32_t>(std::numeric_limits<int>::max()), num_bytes);
  auto buffer =
      base::MakeRefCounted<network::NetToMojoIOBuffer>(pending_write_.get());

  uint32_t bytes_to_read =
      std::min<uint32_t>(num_bytes, info_->response_data_size());

  reader_->ReadData(
      buffer.get(), bytes_to_read,
      base::BindOnce(&AppCacheURLLoader::OnReadComplete, GetWeakPtr()));
}

void AppCacheURLLoader::NotifyCompleted(int error_code) {
  if (storage_.get())
    storage_->CancelDelegateCallbacks(this);

  if (AppCacheRequestHandler::IsRunningInTests())
    return;

  network::URLLoaderCompletionStatus status(error_code);
  if (!error_code) {
    const net::HttpResponseInfo* http_info = is_range_request()
                                                 ? range_response_info_.get()
                                                 : &info_->http_response_info();
    status.exists_in_cache = http_info->was_cached;
    status.completion_time = base::TimeTicks::Now();
    status.encoded_body_length =
        is_range_request() ? range_response_info_->headers->GetContentLength()
                           : info_->response_data_size();
    status.decoded_body_length = status.encoded_body_length;
  }
  client_->OnComplete(status);
}

}  // namespace content
