// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/url_request/url_request_slow_download_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "url/gurl.h"

namespace net {

const char URLRequestSlowDownloadJob::kUnknownSizeUrl[] =
    "http://url.handled.by.slow.download/download-unknown-size";
const char URLRequestSlowDownloadJob::kKnownSizeUrl[] =
    "http://url.handled.by.slow.download/download-known-size";
const char URLRequestSlowDownloadJob::kFinishDownloadUrl[] =
    "http://url.handled.by.slow.download/download-finish";
const char URLRequestSlowDownloadJob::kErrorDownloadUrl[] =
    "http://url.handled.by.slow.download/download-error";

const int URLRequestSlowDownloadJob::kFirstDownloadSize = 1024 * 35;
const int URLRequestSlowDownloadJob::kSecondDownloadSize = 1024 * 10;

class URLRequestSlowDownloadJob::Interceptor : public URLRequestInterceptor {
 public:
  Interceptor() = default;
  ~Interceptor() override = default;

  // URLRequestInterceptor implementation:
  URLRequestJob* MaybeInterceptRequest(
      URLRequest* request,
      NetworkDelegate* network_delegate) const override {
    URLRequestSlowDownloadJob* job =
        new URLRequestSlowDownloadJob(request, network_delegate);
    if (request->url().spec() != kFinishDownloadUrl &&
        request->url().spec() != kErrorDownloadUrl) {
      pending_requests_.Get().insert(job);
    }
    return job;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

// static
base::LazyInstance<URLRequestSlowDownloadJob::SlowJobsSet>::Leaky
    URLRequestSlowDownloadJob::pending_requests_ = LAZY_INSTANCE_INITIALIZER;

void URLRequestSlowDownloadJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestSlowDownloadJob::StartAsync,
                                weak_factory_.GetWeakPtr()));
}

int64_t URLRequestSlowDownloadJob::GetTotalReceivedBytes() const {
  return bytes_already_sent_;
}

// static
void URLRequestSlowDownloadJob::AddUrlHandler() {
  URLRequestFilter* filter = URLRequestFilter::GetInstance();
  filter->AddUrlInterceptor(
      GURL(kUnknownSizeUrl),
      std::unique_ptr<URLRequestInterceptor>(new Interceptor()));
  filter->AddUrlInterceptor(
      GURL(kKnownSizeUrl),
      std::unique_ptr<URLRequestInterceptor>(new Interceptor()));
  filter->AddUrlInterceptor(
      GURL(kFinishDownloadUrl),
      std::unique_ptr<URLRequestInterceptor>(new Interceptor()));
  filter->AddUrlInterceptor(
      GURL(kErrorDownloadUrl),
      std::unique_ptr<URLRequestInterceptor>(new Interceptor()));
}

// static
size_t URLRequestSlowDownloadJob::NumberOutstandingRequests() {
  return pending_requests_.Get().size();
}

// static
void URLRequestSlowDownloadJob::FinishPendingRequests() {
  for (auto it = pending_requests_.Get().begin();
       it != pending_requests_.Get().end(); ++it) {
    (*it)->set_should_finish_download();
  }
}

void URLRequestSlowDownloadJob::ErrorPendingRequests() {
  for (auto it = pending_requests_.Get().begin();
       it != pending_requests_.Get().end(); ++it) {
    (*it)->set_should_error_download();
  }
}

URLRequestSlowDownloadJob::URLRequestSlowDownloadJob(
    URLRequest* request,
    NetworkDelegate* network_delegate)
    : URLRequestJob(request, network_delegate),
      bytes_already_sent_(0),
      should_error_download_(false),
      should_finish_download_(false),
      buffer_size_(0) {}

void URLRequestSlowDownloadJob::StartAsync() {
  if (base::LowerCaseEqualsASCII(kFinishDownloadUrl,
                                 request_->url().spec().c_str()))
    URLRequestSlowDownloadJob::FinishPendingRequests();
  if (base::LowerCaseEqualsASCII(kErrorDownloadUrl,
                                 request_->url().spec().c_str()))
    URLRequestSlowDownloadJob::ErrorPendingRequests();

  NotifyHeadersComplete();
}

// ReadRawData and CheckDoneStatus together implement a state
// machine.  ReadRawData may be called arbitrarily by the network stack.
// It responds by:
//      * If there are bytes remaining in the first chunk, they are
//        returned.
//      [No bytes remaining in first chunk.   ]
//      * If should_finish_download_ is not set, it returns IO_PENDING,
//        and starts calling CheckDoneStatus on a regular timer.
//      [should_finish_download_ set.]
//      * If there are bytes remaining in the second chunk, they are filled.
//      * Otherwise, return *bytes_read = 0 to indicate end of request.
// CheckDoneStatus is called on a regular basis, in the specific
// case where we have transmitted all of the first chunk and none of the
// second.  If should_finish_download_ becomes set, it will "complete"
// the ReadRawData call that spawned off the CheckDoneStatus() repeated call.
//
// FillBufferHelper is a helper function that does the actual work of figuring
// out where in the state machine we are and how we should fill the buffer.
// It returns an enum indicating the state of the read.
URLRequestSlowDownloadJob::ReadStatus
URLRequestSlowDownloadJob::FillBufferHelper(IOBuffer* buf,
                                            int buf_size,
                                            int* bytes_written) {
  if (bytes_already_sent_ < kFirstDownloadSize) {
    int bytes_to_write =
        std::min(kFirstDownloadSize - bytes_already_sent_, buf_size);
    for (int i = 0; i < bytes_to_write; ++i) {
      buf->data()[i] = '*';
    }
    *bytes_written = bytes_to_write;
    bytes_already_sent_ += bytes_to_write;
    return BUFFER_FILLED;
  }

  if (!should_finish_download_)
    return REQUEST_BLOCKED;

  if (bytes_already_sent_ < kFirstDownloadSize + kSecondDownloadSize) {
    int bytes_to_write =
        std::min(kFirstDownloadSize + kSecondDownloadSize - bytes_already_sent_,
                 buf_size);
    for (int i = 0; i < bytes_to_write; ++i) {
      buf->data()[i] = '*';
    }
    *bytes_written = bytes_to_write;
    bytes_already_sent_ += bytes_to_write;
    return BUFFER_FILLED;
  }

  return REQUEST_COMPLETE;
}

int URLRequestSlowDownloadJob::ReadRawData(IOBuffer* buf, int buf_size) {
  if (base::LowerCaseEqualsASCII(kFinishDownloadUrl,
                                 request_->url().spec().c_str()) ||
      base::LowerCaseEqualsASCII(kErrorDownloadUrl,
                                 request_->url().spec().c_str())) {
    VLOG(10) << __FUNCTION__ << " called w/ kFinish/ErrorDownloadUrl.";
    return 0;
  }

  VLOG(10) << __FUNCTION__ << " called at position " << bytes_already_sent_
           << " in the stream.";
  int bytes_read = 0;
  ReadStatus status = FillBufferHelper(buf, buf_size, &bytes_read);
  switch (status) {
    case BUFFER_FILLED:
    case REQUEST_COMPLETE:
      return bytes_read;
    case REQUEST_BLOCKED:
      buffer_ = buf;
      buffer_size_ = buf_size;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&URLRequestSlowDownloadJob::CheckDoneStatus,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(100));
      return ERR_IO_PENDING;
  }
  NOTREACHED();
  return OK;
}

void URLRequestSlowDownloadJob::CheckDoneStatus() {
  if (should_finish_download_) {
    VLOG(10) << __FUNCTION__ << " called w/ should_finish_download_ set.";
    DCHECK(nullptr != buffer_.get());
    int bytes_written = 0;
    ReadStatus status =
        FillBufferHelper(buffer_.get(), buffer_size_, &bytes_written);
    DCHECK_EQ(BUFFER_FILLED, status);
    buffer_ = nullptr;  // Release the reference.
    ReadRawDataComplete(bytes_written);
  } else if (should_error_download_) {
    VLOG(10) << __FUNCTION__ << " called w/ should_finish_ownload_ set.";
    ReadRawDataComplete(ERR_CONNECTION_RESET);
  } else {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&URLRequestSlowDownloadJob::CheckDoneStatus,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(100));
  }
}

// Public virtual version.
void URLRequestSlowDownloadJob::GetResponseInfo(HttpResponseInfo* info) {
  // Forward to private const version.
  GetResponseInfoConst(info);
}

URLRequestSlowDownloadJob::~URLRequestSlowDownloadJob() {
  pending_requests_.Get().erase(this);
}

// Private const version.
void URLRequestSlowDownloadJob::GetResponseInfoConst(
    HttpResponseInfo* info) const {
  // Send back mock headers.
  std::string raw_headers;
  if (base::LowerCaseEqualsASCII(kFinishDownloadUrl,
                                 request_->url().spec().c_str()) ||
      base::LowerCaseEqualsASCII(kErrorDownloadUrl,
                                 request_->url().spec().c_str())) {
    raw_headers.append(
        "HTTP/1.1 200 OK\n"
        "Content-type: text/plain\n");
  } else {
    raw_headers.append(
        "HTTP/1.1 200 OK\n"
        "Content-type: application/octet-stream\n"
        "Cache-Control: max-age=0\n");

    if (base::LowerCaseEqualsASCII(kKnownSizeUrl,
                                   request_->url().spec().c_str())) {
      raw_headers.append(base::StringPrintf(
          "Content-Length: %d\n", kFirstDownloadSize + kSecondDownloadSize));
    }
  }

  // ParseRawHeaders expects \0 to end each header line.
  base::ReplaceSubstringsAfterOffset(
      &raw_headers, 0, "\n", base::StringPiece("\0", 1));
  info->headers = new HttpResponseHeaders(raw_headers);
}

bool URLRequestSlowDownloadJob::GetMimeType(std::string* mime_type) const {
  HttpResponseInfo info;
  GetResponseInfoConst(&info);
  return info.headers.get() && info.headers->GetMimeType(mime_type);
}

}  // namespace net
