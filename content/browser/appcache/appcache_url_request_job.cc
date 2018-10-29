// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_url_request_job.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_histograms.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_service_impl.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace content {

AppCacheURLRequestJob::~AppCacheURLRequestJob() {
  if (storage_)
    storage_->CancelDelegateCallbacks(this);
}

void AppCacheURLRequestJob::Kill() {
  if (!has_been_killed_) {
    has_been_killed_ = true;
    reader_.reset();
    handler_source_reader_.reset();
    if (storage_) {
      storage_->CancelDelegateCallbacks(this);
      storage_ = nullptr;
    }
    host_ = nullptr;
    info_ = nullptr;
    cache_ = nullptr;
    group_ = nullptr;
    range_response_info_.reset();
    net::URLRequestJob::Kill();
    weak_factory_.InvalidateWeakPtrs();
  }
}

bool AppCacheURLRequestJob::IsStarted() const {
  return has_been_started_;
}

void AppCacheURLRequestJob::DeliverAppCachedResponse(const GURL& manifest_url,
                                                     int64_t cache_id,
                                                     const AppCacheEntry& entry,
                                                     bool is_fallback) {
  DCHECK(!has_delivery_orders());
  DCHECK(entry.has_response_id());
  delivery_type_ = DeliveryType::kAppCached;
  manifest_url_ = manifest_url;
  cache_id_ = cache_id;
  entry_ = entry;
  is_fallback_ = is_fallback;
  MaybeBeginDelivery();
}

void AppCacheURLRequestJob::DeliverNetworkResponse() {
  DCHECK(!has_delivery_orders());
  delivery_type_ = DeliveryType::kNetwork;
  storage_ = nullptr;  // not needed
  MaybeBeginDelivery();
}

void AppCacheURLRequestJob::DeliverErrorResponse() {
  DCHECK(!has_delivery_orders());
  delivery_type_ = DeliveryType::kError;
  storage_ = nullptr;  // not needed
  MaybeBeginDelivery();
}

AppCacheURLRequestJob* AppCacheURLRequestJob::AsURLRequestJob() {
  return this;
}

base::WeakPtr<AppCacheJob> AppCacheURLRequestJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

base::WeakPtr<AppCacheURLRequestJob>
AppCacheURLRequestJob::GetDerivedWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

AppCacheURLRequestJob::AppCacheURLRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    AppCacheStorage* storage,
    AppCacheHost* host,
    bool is_main_resource,
    OnPrepareToRestartCallback restart_callback)
    : net::URLRequestJob(request, network_delegate),
      host_(host),
      storage_(storage),
      has_been_started_(false),
      has_been_killed_(false),
      cache_id_(kAppCacheNoCacheId),
      is_fallback_(false),
      is_main_resource_(is_main_resource),
      on_prepare_to_restart_callback_(std::move(restart_callback)),
      weak_factory_(this) {
  DCHECK(storage_);
}

void AppCacheURLRequestJob::MaybeBeginDelivery() {
  if (IsStarted() && has_delivery_orders()) {
    // Start asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppCacheURLRequestJob::BeginDelivery,
                                  GetDerivedWeakPtr()));
  }
}

void AppCacheURLRequestJob::BeginDelivery() {
  DCHECK(has_delivery_orders() && IsStarted());

  if (has_been_killed())
    return;

  switch (delivery_type_) {
    case DeliveryType::kNetwork:
      // To fallthru to the network, we restart the request which will
      // cause a new job to be created to retrieve the resource from the
      // network. Our caller is responsible for arranging to not re-intercept
      // the same request.
      NotifyRestartRequired();
      break;

    case DeliveryType::kError:
      request()->net_log().AddEvent(
          net::NetLogEventType::APPCACHE_DELIVERING_ERROR_RESPONSE);
      NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                             net::ERR_FAILED));
      break;

    case DeliveryType::kAppCached:
      request()->net_log().AddEvent(
          is_fallback_
              ? net::NetLogEventType::APPCACHE_DELIVERING_FALLBACK_RESPONSE
              : net::NetLogEventType::APPCACHE_DELIVERING_CACHED_RESPONSE);
      storage_->LoadResponseInfo(manifest_url_, entry_.response_id(), this);
      break;

    default:
      NOTREACHED();
      break;
  }
}

void AppCacheURLRequestJob::BeginErrorDelivery(const char* message) {
  if (host_)
    host_->frontend()->OnLogMessage(host_->host_id(), APPCACHE_LOG_ERROR,
                                    message);
  delivery_type_ = DeliveryType::kError;
  storage_ = nullptr;
  BeginDelivery();
}

void AppCacheURLRequestJob::OnResponseInfoLoaded(
    AppCacheResponseInfo* response_info,
    int64_t response_id) {
  DCHECK(IsDeliveringAppCacheResponse());
  if (response_info) {
    info_ = response_info;
    reader_ =
        storage_->CreateResponseReader(manifest_url_, entry_.response_id());

    if (is_range_request())
      SetupRangeResponse();

    NotifyHeadersComplete();
  } else {
    if (storage_->service()->storage() == storage_) {
      // A resource that is expected to be in the appcache is missing.
      // See http://code.google.com/p/chromium/issues/detail?id=50657
      // Instead of failing the request, we restart the request. The retry
      // attempt will fallthru to the network instead of trying to load
      // from the appcache.
      storage_->service()->CheckAppCacheResponse(manifest_url_, cache_id_,
                                                 entry_.response_id());
      AppCacheHistograms::CountResponseRetrieval(
          false, is_main_resource_, url::Origin::Create(manifest_url_));
    }
    cache_entry_not_found_ = true;

    // We fallback to the network unless this job was falling back to the
    // appcache from the network which had already failed in some way.
    if (!is_fallback_)
      NotifyRestartRequired();
    else
      BeginErrorDelivery("failed to load appcache response info");
  }
}

const net::HttpResponseInfo* AppCacheURLRequestJob::http_info() const {
  if (!info_.get())
    return nullptr;
  if (range_response_info_)
    return range_response_info_.get();
  return &info_->http_response_info();
}

void AppCacheURLRequestJob::OnReadComplete(int result) {
  DCHECK(IsDeliveringAppCacheResponse());
  if (result == 0) {
    AppCacheHistograms::CountResponseRetrieval(
        true, is_main_resource_, url::Origin::Create(manifest_url_));
  } else if (result < 0) {
    if (storage_->service()->storage() == storage_) {
      storage_->service()->CheckAppCacheResponse(manifest_url_, cache_id_,
                                                 entry_.response_id());
    }
    AppCacheHistograms::CountResponseRetrieval(
        false, is_main_resource_, url::Origin::Create(manifest_url_));
  }
  ReadRawDataComplete(result);
}

// net::URLRequestJob overrides ------------------------------------------------

void AppCacheURLRequestJob::Start() {
  DCHECK(!IsStarted());
  has_been_started_ = true;
  start_time_tick_ = base::TimeTicks::Now();
  MaybeBeginDelivery();
}

net::LoadState AppCacheURLRequestJob::GetLoadState() const {
  if (!IsStarted())
    return net::LOAD_STATE_IDLE;
  if (!has_delivery_orders())
    return net::LOAD_STATE_WAITING_FOR_APPCACHE;
  if (delivery_type_ != DeliveryType::kAppCached)
    return net::LOAD_STATE_IDLE;
  if (!info_.get())
    return net::LOAD_STATE_WAITING_FOR_APPCACHE;
  if (reader_.get() && reader_->IsReadPending())
    return net::LOAD_STATE_READING_RESPONSE;
  return net::LOAD_STATE_IDLE;
}

bool AppCacheURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

bool AppCacheURLRequestJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

void AppCacheURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
}

int AppCacheURLRequestJob::ReadRawData(net::IOBuffer* buf, int buf_size) {
  DCHECK(IsDeliveringAppCacheResponse());
  DCHECK_NE(buf_size, 0);
  DCHECK(!reader_->IsReadPending());
  reader_->ReadData(buf, buf_size,
                    base::BindOnce(&AppCacheURLRequestJob::OnReadComplete,
                                   base::Unretained(this)));
  return net::ERR_IO_PENDING;
}

net::HostPortPair AppCacheURLRequestJob::GetSocketAddress() const {
  if (!http_info())
    return net::HostPortPair();
  return http_info()->socket_address;
}

void AppCacheURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  InitializeRangeRequestInfo(headers);
}

void AppCacheURLRequestJob::NotifyRestartRequired() {
  std::move(on_prepare_to_restart_callback_).Run();
  URLRequestJob::NotifyRestartRequired();
}

}  // namespace content
