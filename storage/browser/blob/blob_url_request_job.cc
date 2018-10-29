// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/blob/blob_url_request_job.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_status.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_reader.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_url.h"

namespace storage {

BlobURLRequestJob::BlobURLRequestJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate,
                                     BlobDataHandle* blob_handle)
    : net::URLRequestJob(request, network_delegate),
      error_(false),
      byte_range_set_(false),
      weak_factory_(this) {
  TRACE_EVENT_ASYNC_BEGIN1("Blob", "BlobRequest", this, "uuid",
                           blob_handle ? blob_handle->uuid() : "NotFound");
  if (blob_handle) {
    blob_handle_.reset(new BlobDataHandle(*blob_handle));
    blob_reader_ = blob_handle_->CreateReader();
  }
}

void BlobURLRequestJob::Start() {
  // Continue asynchronously.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&BlobURLRequestJob::DidStart, weak_factory_.GetWeakPtr()));
}

void BlobURLRequestJob::Kill() {
  if (blob_reader_) {
    blob_reader_->Kill();
  }
  net::URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

int BlobURLRequestJob::ReadRawData(net::IOBuffer* dest, int dest_size) {
  TRACE_EVENT_ASYNC_BEGIN1("Blob", "BlobRequest::ReadRawData", this, "uuid",
                           blob_handle_ ? blob_handle_->uuid() : "NotFound");
  DCHECK_NE(dest_size, 0);

  // Bail out immediately if we encounter an error. This happens if a previous
  // ReadRawData signalled an error to its caller but the caller called
  // ReadRawData again anyway.
  if (error_)
    return 0;

  int bytes_read = 0;
  BlobReader::Status read_status =
      blob_reader_->Read(dest, dest_size, &bytes_read,
                         base::BindOnce(&BlobURLRequestJob::DidReadRawData,
                                        weak_factory_.GetWeakPtr()));

  switch (read_status) {
    case BlobReader::Status::NET_ERROR:
      TRACE_EVENT_ASYNC_END1("Blob", "BlobRequest::ReadRawData", this, "uuid",
                             blob_handle_ ? blob_handle_->uuid() : "NotFound");
      return blob_reader_->net_error();
    case BlobReader::Status::IO_PENDING:
      return net::ERR_IO_PENDING;
    case BlobReader::Status::DONE:
      TRACE_EVENT_ASYNC_END1("Blob", "BlobRequest::ReadRawData", this, "uuid",
                             blob_handle_ ? blob_handle_->uuid() : "NotFound");
      return bytes_read;
  }
  NOTREACHED();
  return 0;
}

bool BlobURLRequestJob::GetMimeType(std::string* mime_type) const {
  if (!response_info_)
    return false;

  return response_info_->headers->GetMimeType(mime_type);
}

void BlobURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (response_info_)
    *info = *response_info_;
}

void BlobURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    // We only care about "Range" header here.
    std::vector<net::HttpByteRange> ranges;
    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_set_ = true;
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request,
        // because we need to do multipart encoding here.
        // TODO(jianli): Support multipart byte range requests.
        NotifyFailure(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
      }
    }
  }
}

scoped_refptr<net::HttpResponseHeaders> BlobURLRequestJob::GenerateHeaders(
    net::HttpStatusCode status_code,
    BlobDataHandle* blob_handle,
    net::HttpByteRange* byte_range,
    uint64_t total_size,
    uint64_t content_size) {
  std::string status("HTTP/1.1 ");
  status.append(base::IntToString(status_code));
  status.append(" ");
  status.append(net::GetHttpReasonPhrase(status_code));
  status.append("\0\0", 2);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(status);

  if (status_code == net::HTTP_OK || status_code == net::HTTP_PARTIAL_CONTENT) {
    std::string content_length_header(net::HttpRequestHeaders::kContentLength);
    content_length_header.append(": ");
    content_length_header.append(base::NumberToString(content_size));
    headers->AddHeader(content_length_header);
    if (status_code == net::HTTP_PARTIAL_CONTENT) {
      DCHECK(byte_range->IsValid());
      std::string content_range_header(net::HttpResponseHeaders::kContentRange);
      content_range_header.append(": bytes ");
      content_range_header.append(base::StringPrintf(
          "%" PRId64 "-%" PRId64, byte_range->first_byte_position(),
          byte_range->last_byte_position()));
      content_range_header.append("/");
      content_range_header.append(base::StringPrintf("%" PRId64, total_size));
      headers->AddHeader(content_range_header);
    }
    if (!blob_handle->content_type().empty()) {
      std::string content_type_header(net::HttpRequestHeaders::kContentType);
      content_type_header.append(": ");
      content_type_header.append(blob_handle->content_type());
      headers->AddHeader(content_type_header);
    }
    if (!blob_handle->content_disposition().empty()) {
      std::string content_disposition_header("Content-Disposition: ");
      content_disposition_header.append(blob_handle->content_disposition());
      headers->AddHeader(content_disposition_header);
    }
  }

  return headers;
}

BlobURLRequestJob::~BlobURLRequestJob() {
  TRACE_EVENT_ASYNC_END1("Blob", "BlobRequest", this, "uuid",
                         blob_handle_ ? blob_handle_->uuid() : "NotFound");
}

void BlobURLRequestJob::DidStart() {
  error_ = false;

  // We only support GET request per the spec.
  if (request()->method() != "GET") {
    NotifyFailure(net::ERR_METHOD_NOT_SUPPORTED);
    return;
  }

  // If the blob data is not present, bail out.
  if (!blob_handle_) {
    NotifyFailure(net::ERR_FILE_NOT_FOUND);
    return;
  }
  if (blob_reader_->net_error()) {
    NotifyFailure(blob_reader_->net_error());
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN1("Blob", "BlobRequest::CountSize", this, "uuid",
                           blob_handle_->uuid());
  BlobReader::Status size_status = blob_reader_->CalculateSize(base::BindOnce(
      &BlobURLRequestJob::DidCalculateSize, weak_factory_.GetWeakPtr()));
  switch (size_status) {
    case BlobReader::Status::NET_ERROR:
      NotifyFailure(blob_reader_->net_error());
      return;
    case BlobReader::Status::IO_PENDING:
      return;
    case BlobReader::Status::DONE:
      DidCalculateSize(net::OK);
      return;
  }
}

void BlobURLRequestJob::DidCalculateSize(int result) {
  TRACE_EVENT_ASYNC_END1("Blob", "BlobRequest::CountSize", this, "uuid",
                         blob_handle_->uuid());

  if (result != net::OK) {
    NotifyFailure(result);
    return;
  }

  // Apply the range requirement.
  if (!byte_range_.ComputeBounds(blob_reader_->total_size())) {
    NotifyFailure(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE);
    return;
  }

  DCHECK_LE(byte_range_.first_byte_position(),
            byte_range_.last_byte_position() + 1);
  uint64_t length = base::checked_cast<uint64_t>(
      byte_range_.last_byte_position() - byte_range_.first_byte_position() + 1);

  if (byte_range_set_)
    blob_reader_->SetReadRange(byte_range_.first_byte_position(), length);

  net::HttpStatusCode status_code = net::HTTP_OK;
  if (byte_range_set_ && byte_range_.IsValid()) {
    status_code = net::HTTP_PARTIAL_CONTENT;
  } else {
    // TODO(horo): When the requester doesn't need the side data (ex:FileReader)
    // we should skip reading the side data.
    if (blob_reader_->has_side_data() &&
        blob_reader_->ReadSideData(base::BindOnce(
            &BlobURLRequestJob::DidReadMetadata, weak_factory_.GetWeakPtr())) ==
            BlobReader::Status::IO_PENDING) {
      return;
    }
  }

  HeadersCompleted(status_code);
}

void BlobURLRequestJob::DidReadMetadata(BlobReader::Status result) {
  if (result != BlobReader::Status::DONE) {
    NotifyFailure(blob_reader_->net_error());
    return;
  }
  HeadersCompleted(net::HTTP_OK);
}

void BlobURLRequestJob::DidReadRawData(int result) {
  TRACE_EVENT_ASYNC_END1("Blob", "BlobRequest::ReadRawData", this, "uuid",
                         blob_handle_ ? blob_handle_->uuid() : "NotFound");
  ReadRawDataComplete(result);
}

void BlobURLRequestJob::NotifyFailure(int error_code) {
  error_ = true;

  // If we already return the headers on success, we can't change the headers
  // now. Instead, we just error out.
  DCHECK(!response_info_) << "Cannot NotifyFailure after headers.";

  NotifyStartError(net::URLRequestStatus::FromError(error_code));
}

void BlobURLRequestJob::HeadersCompleted(net::HttpStatusCode status_code) {
  uint64_t content_size = 0;
  uint64_t total_size = 0;
  if (status_code == net::HTTP_OK || status_code == net::HTTP_PARTIAL_CONTENT) {
    content_size = blob_reader_->remaining_bytes();
    set_expected_content_size(content_size);
    total_size = blob_reader_->total_size();
  }
  response_info_.reset(new net::HttpResponseInfo());
  response_info_->headers = GenerateHeaders(
      status_code, blob_handle_.get(), &byte_range_, total_size, content_size);
  if (blob_reader_)
    response_info_->metadata = blob_reader_->side_data();

  NotifyHeadersComplete();
}

}  // namespace storage
