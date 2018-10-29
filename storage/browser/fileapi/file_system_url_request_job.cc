// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/fileapi/file_system_url_request_job.h"

#include <stddef.h>

#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "storage/browser/fileapi/file_stream_reader.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/common/fileapi/file_system_util.h"
#include "url/gurl.h"

using net::NetworkDelegate;
using net::URLRequest;
using net::URLRequestJob;
using net::URLRequestStatus;

namespace storage {

static net::HttpResponseHeaders* CreateHttpResponseHeaders() {
  // HttpResponseHeaders expects its input string to be terminated by two NULs.
  static const char kStatus[] = "HTTP/1.1 200 OK\0";
  static const size_t kStatusLen = arraysize(kStatus);

  net::HttpResponseHeaders* headers =
      new net::HttpResponseHeaders(std::string(kStatus, kStatusLen));

  // Tell WebKit never to cache this content.
  std::string cache_control(net::HttpRequestHeaders::kCacheControl);
  cache_control.append(": no-cache");
  headers->AddHeader(cache_control);

  return headers;
}

FileSystemURLRequestJob::FileSystemURLRequestJob(
    URLRequest* request,
    NetworkDelegate* network_delegate,
    const std::string& storage_domain,
    FileSystemContext* file_system_context)
    : URLRequestJob(request, network_delegate),
      storage_domain_(storage_domain),
      file_system_context_(file_system_context),
      is_directory_(false),
      remaining_bytes_(0),
      range_parse_result_(net::OK),
      weak_factory_(this) {}

FileSystemURLRequestJob::~FileSystemURLRequestJob() = default;

void FileSystemURLRequestJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FileSystemURLRequestJob::StartAsync,
                                weak_factory_.GetWeakPtr()));
}

void FileSystemURLRequestJob::Kill() {
  reader_.reset();
  URLRequestJob::Kill();
  weak_factory_.InvalidateWeakPtrs();
}

int FileSystemURLRequestJob::ReadRawData(net::IOBuffer* dest, int dest_size) {
  DCHECK_NE(dest_size, 0);
  DCHECK_GE(remaining_bytes_, 0);

  if (reader_.get() == nullptr)
    return net::ERR_FAILED;

  if (remaining_bytes_ < dest_size)
    dest_size = remaining_bytes_;

  if (!dest_size)
    return 0;

  const int rv = reader_->Read(dest, dest_size,
                               base::BindOnce(&FileSystemURLRequestJob::DidRead,
                                              weak_factory_.GetWeakPtr()));
  if (rv >= 0) {
    remaining_bytes_ -= rv;
    DCHECK_GE(remaining_bytes_, 0);
  }

  return rv;
}

bool FileSystemURLRequestJob::GetMimeType(std::string* mime_type) const {
  DCHECK(request_);
  DCHECK(url_.is_valid());
  base::FilePath::StringType extension = url_.path().Extension();
  if (!extension.empty())
    extension = extension.substr(1);
  return net::GetWellKnownMimeTypeFromExtension(extension, mime_type);
}

void FileSystemURLRequestJob::SetExtraRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  std::string range_header;
  // Currently this job only cares about the Range header. Note that validation
  // is deferred to DidGetMetaData(), because NotifyStartError is not legal to
  // call since the job has not started.
  if (headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header)) {
    std::vector<net::HttpByteRange> ranges;

    if (net::HttpUtil::ParseRangeHeader(range_header, &ranges)) {
      if (ranges.size() == 1) {
        byte_range_ = ranges[0];
      } else {
        // We don't support multiple range requests in one single URL request.
        // TODO(adamk): decide whether we want to support multiple range
        // requests.
        range_parse_result_ = net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;
      }
    }
  }
}

void FileSystemURLRequestJob::GetResponseInfo(net::HttpResponseInfo* info) {
  if (response_info_)
    *info = *response_info_;
}

void FileSystemURLRequestJob::StartAsync() {
  if (!request_)
    return;
  DCHECK(!reader_.get());
  url_ = file_system_context_->CrackURL(request_->url());
  if (!url_.is_valid()) {
    const FileSystemRequestInfo& request_info = {request_->url(), request_,
                                                 storage_domain_, 0};
    file_system_context_->AttemptAutoMountForURLRequest(
        request_info,
        base::BindOnce(&FileSystemURLRequestJob::DidAttemptAutoMount,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  if (!file_system_context_->CanServeURLRequest(url_)) {
    // In incognito mode the API is not usable and there should be no data.
    NotifyStartError(URLRequestStatus::FromError(net::ERR_FILE_NOT_FOUND));
    return;
  }
  file_system_context_->operation_runner()->GetMetadata(
      url_,
      FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          FileSystemOperation::GET_METADATA_FIELD_SIZE,
      base::BindOnce(&FileSystemURLRequestJob::DidGetMetadata,
                     weak_factory_.GetWeakPtr()));
}

void FileSystemURLRequestJob::DidAttemptAutoMount(base::File::Error result) {
  if (result >= 0 &&
      file_system_context_->CrackURL(request_->url()).is_valid()) {
    StartAsync();
  } else {
    NotifyStartError(URLRequestStatus::FromError(net::ERR_FILE_NOT_FOUND));
  }
}

void FileSystemURLRequestJob::DidGetMetadata(
    base::File::Error error_code,
    const base::File::Info& file_info) {
  if (error_code != base::File::FILE_OK) {
    NotifyStartError(URLRequestStatus::FromError(
        error_code == base::File::FILE_ERROR_INVALID_URL
            ? net::ERR_INVALID_URL
            : net::ERR_FILE_NOT_FOUND));
    return;
  }

  // We may have been orphaned...
  if (!request_)
    return;

  is_directory_ = file_info.is_directory;

  if (range_parse_result_ != net::OK) {
    NotifyStartError(URLRequestStatus::FromError(range_parse_result_));
    return;
  }

  if (!byte_range_.ComputeBounds(file_info.size)) {
    NotifyStartError(
        URLRequestStatus::FromError(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE));
    return;
  }

  if (is_directory_) {
    NotifyHeadersComplete();
    return;
  }

  remaining_bytes_ = byte_range_.last_byte_position() -
                     byte_range_.first_byte_position() + 1;
  DCHECK_GE(remaining_bytes_, 0);

  DCHECK(!reader_.get());
  reader_ = file_system_context_->CreateFileStreamReader(
      url_, byte_range_.first_byte_position(), remaining_bytes_, base::Time());

  set_expected_content_size(remaining_bytes_);
  response_info_.reset(new net::HttpResponseInfo());
  response_info_->headers = CreateHttpResponseHeaders();
  NotifyHeadersComplete();
}

void FileSystemURLRequestJob::DidRead(int result) {
  if (result >= 0) {
    remaining_bytes_ -= result;
    DCHECK_GE(remaining_bytes_, 0);
  }

  ReadRawDataComplete(result);
}

bool FileSystemURLRequestJob::IsRedirectResponse(
    GURL* location,
    int* http_status_code,
    bool* insecure_scheme_was_upgraded) {
  if (is_directory_) {
    // This happens when we discovered the file is a directory, so needs a
    // slash at the end of the path.
    std::string new_path = request_->url().path();
    new_path.push_back('/');
    GURL::Replacements replacements;
    replacements.SetPathStr(new_path);
    *insecure_scheme_was_upgraded = false;
    *location = request_->url().ReplaceComponents(replacements);
    *http_status_code = 301;  // simulate a permanent redirect
    return true;
  }

  return false;
}

}  // namespace storage
