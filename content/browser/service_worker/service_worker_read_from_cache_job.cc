// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_read_from_cache_job.h"

#include <string>
#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"

namespace content {

ServiceWorkerReadFromCacheJob::ServiceWorkerReadFromCacheJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    ResourceType resource_type,
    base::WeakPtr<ServiceWorkerContextCore> context,
    const scoped_refptr<ServiceWorkerVersion>& version,
    int64_t resource_id)
    : net::URLRequestJob(request, network_delegate),
      resource_type_(resource_type),
      resource_id_(resource_id),
      context_(context),
      version_(version),
      weak_factory_(this) {
  DCHECK(version_);
#if DCHECK_IS_ON()
  switch (version_->script_type()) {
    case blink::mojom::ScriptType::kClassic:
      // For classic scripts, the main service worker script should have the
      // "service worker" resource type and imported scripts should have the
      // "script" resource type.
      DCHECK(resource_type_ == RESOURCE_TYPE_SCRIPT ||
             (resource_type_ == RESOURCE_TYPE_SERVICE_WORKER &&
              version_->script_url() == request_->url()));
      break;
    case blink::mojom::ScriptType::kModule:
      // For module scripts, both the main service worker script and
      // static-imported scripts should have the "service worker" resource type
      // because static import inherits the resource type of the top-level
      // module script.
      DCHECK_EQ(RESOURCE_TYPE_SERVICE_WORKER, resource_type_);
      break;
  }
#endif  // DCHECK_IS_ON()
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                    "ServiceWorkerReadFromCacheJob", this,
                                    "URL", request_->url().spec());
}

ServiceWorkerReadFromCacheJob::~ServiceWorkerReadFromCacheJob() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker",
                                  "ServiceWorkerReadFromCacheJob", this);
}

void ServiceWorkerReadFromCacheJob::Start() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "ReadInfo", this);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerReadFromCacheJob::StartAsync,
                                weak_factory_.GetWeakPtr()));
}

void ServiceWorkerReadFromCacheJob::Kill() {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("ServiceWorker", "Kill", this);
  if (has_been_killed_)
    return;
  weak_factory_.InvalidateWeakPtrs();
  has_been_killed_ = true;
  reader_.reset();
  context_.reset();
  http_info_io_buffer_ = nullptr;
  http_info_.reset();
  range_response_info_.reset();
  net::URLRequestJob::Kill();
}

net::LoadState ServiceWorkerReadFromCacheJob::GetLoadState() const {
  if (reader_.get() && reader_->IsReadPending())
    return net::LOAD_STATE_READING_RESPONSE;
  return net::LOAD_STATE_IDLE;
}

bool ServiceWorkerReadFromCacheJob::GetCharset(std::string* charset) {
  if (!http_info())
    return false;
  return http_info()->headers->GetCharset(charset);
}

bool ServiceWorkerReadFromCacheJob::GetMimeType(std::string* mime_type) const {
  if (!http_info())
    return false;
  return http_info()->headers->GetMimeType(mime_type);
}

void ServiceWorkerReadFromCacheJob::GetResponseInfo(
    net::HttpResponseInfo* info) {
  if (!http_info())
    return;
  *info = *http_info();
}

void ServiceWorkerReadFromCacheJob::SetExtraRequestHeaders(
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

int ServiceWorkerReadFromCacheJob::ReadRawData(net::IOBuffer* buf,
                                               int buf_size) {
  DCHECK_NE(buf_size, 0);
  DCHECK(!reader_->IsReadPending());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker", "ReadRawData", this,
                                    "buf_size", buf_size);
  reader_->ReadData(
      buf, buf_size,
      base::BindOnce(&ServiceWorkerReadFromCacheJob::OnReadComplete,
                     weak_factory_.GetWeakPtr()));
  return net::ERR_IO_PENDING;
}

void ServiceWorkerReadFromCacheJob::StartAsync() {
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT0("ServiceWorker", "StartAsync", this);
  if (!context_) {
    // NotifyStartError is not safe to call synchronously in Start.
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, net::ERR_FAILED));
    return;
  }

  // Create a response reader and start reading the headers,
  // we'll continue when thats done.
  if (is_main_script())
    version_->embedded_worker()->OnScriptReadStarted();
  reader_ = context_->storage()->CreateResponseReader(resource_id_);
  http_info_io_buffer_ = new HttpResponseInfoIOBuffer;
  reader_->ReadInfo(
      http_info_io_buffer_.get(),
      base::BindOnce(&ServiceWorkerReadFromCacheJob::OnReadInfoComplete,
                     weak_factory_.GetWeakPtr()));
}

const net::HttpResponseInfo* ServiceWorkerReadFromCacheJob::http_info() const {
  if (!http_info_)
    return nullptr;
  if (range_response_info_)
    return range_response_info_.get();
  return http_info_.get();
}

void ServiceWorkerReadFromCacheJob::OnReadInfoComplete(int result) {
  if (!http_info_io_buffer_->http_info) {
    DCHECK_LT(result, 0);
    ServiceWorkerMetrics::CountReadResponseResult(
        ServiceWorkerMetrics::READ_HEADERS_ERROR);
    Done(net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
    NotifyStartError(
        net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
    return;
  }
  DCHECK_GE(result, 0);
  http_info_.reset(http_info_io_buffer_->http_info.release());
  if (is_range_request())
    SetupRangeResponse(http_info_io_buffer_->response_data_size);
  http_info_io_buffer_ = nullptr;
  if (is_main_script())
    version_->SetMainScriptHttpResponseInfo(*http_info_);
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "ReadInfo", this, "Result",
                                  result);
  NotifyHeadersComplete();
}

void ServiceWorkerReadFromCacheJob::SetupRangeResponse(int resource_size) {
  DCHECK(is_range_request() && http_info_.get() && reader_.get());
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
  range_response_info_.reset(new net::HttpResponseInfo(*http_info_));
  net::HttpResponseHeaders* headers = range_response_info_->headers.get();
  headers->UpdateWithNewRange(
      range_requested_, resource_size, true /* replace status line */);
}

void ServiceWorkerReadFromCacheJob::Done(const net::URLRequestStatus& status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!status.is_success()) {
    version_->SetStartWorkerStatusCode(
        blink::ServiceWorkerStatusCode::kErrorDiskCache);
    // TODO(falken): Retry before evicting.
    if (context_) {
      ServiceWorkerRegistration* registration =
          context_->GetLiveRegistration(version_->registration_id());
      registration->DeleteVersion(version_);
    }
  }
  if (is_main_script())
    version_->embedded_worker()->OnScriptReadFinished();
}

void ServiceWorkerReadFromCacheJob::OnReadComplete(int result) {
  ServiceWorkerMetrics::ReadResponseResult check_result;

  if (result >= 0) {
    check_result = ServiceWorkerMetrics::READ_OK;
    if (result == 0)
      Done(net::URLRequestStatus());
  } else {
    check_result = ServiceWorkerMetrics::READ_DATA_ERROR;
    Done(net::URLRequestStatus(net::URLRequestStatus::FAILED, result));
  }

  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "ReadRawData", this,
                                  "Result", result);
  ServiceWorkerMetrics::CountReadResponseResult(check_result);
  ReadRawDataComplete(result);
}

}  // namespace content
