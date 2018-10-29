// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This class simulates what wininet does when a dns lookup fails.

#include <algorithm>
#include <cstring>

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/test/net/url_request_abort_on_end_job.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_status.h"

namespace content {
namespace {

const char kPageContent[] = "some data\r\n";

class Interceptor : public net::URLRequestInterceptor {
 public:
  Interceptor() {}
  ~Interceptor() override {}

  // URLRequestInterceptor implementation:
  net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    return new URLRequestAbortOnEndJob(request, network_delegate);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Interceptor);
};

void AddUrlHandlerOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddUrlInterceptor(
      GURL(URLRequestAbortOnEndJob::k400AbortOnEndUrl),
      std::unique_ptr<net::URLRequestInterceptor>(new Interceptor()));
}

}  // anonymous namespace

const char URLRequestAbortOnEndJob::k400AbortOnEndUrl[] =
    "http://url.handled.by.abort.on.end/400";

// static
void URLRequestAbortOnEndJob::AddUrlHandler() {
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::IO},
                           base::BindOnce(AddUrlHandlerOnIOThread));
}

// Private const version.
void URLRequestAbortOnEndJob::GetResponseInfoConst(
    net::HttpResponseInfo* info) const {
  // Send back mock headers.
  std::string raw_headers;
  if (base::LowerCaseEqualsASCII(k400AbortOnEndUrl,
                                 request_->url().spec().c_str())) {
    raw_headers.append(
      "HTTP/1.1 400 This is not OK\n"
      "Content-type: text/plain\n");
  } else {
    NOTREACHED();
  }
  // ParseRawHeaders expects \0 to end each header line.
  base::ReplaceSubstringsAfterOffset(
      &raw_headers, 0, "\n", base::StringPiece("\0", 1));
  info->headers = new net::HttpResponseHeaders(raw_headers);
}

URLRequestAbortOnEndJob::URLRequestAbortOnEndJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate)
    : URLRequestJob(request, network_delegate),
      sent_data_(false),
      weak_factory_(this) {
}

URLRequestAbortOnEndJob::~URLRequestAbortOnEndJob() {
}

void URLRequestAbortOnEndJob::GetResponseInfo(net::HttpResponseInfo* info) {
  GetResponseInfoConst(info);
}

bool URLRequestAbortOnEndJob::GetMimeType(std::string* mime_type) const {
  net::HttpResponseInfo info;
  GetResponseInfoConst(&info);
  return info.headers.get() && info.headers->GetMimeType(mime_type);
}

void URLRequestAbortOnEndJob::StartAsync() {
  NotifyHeadersComplete();
}

void URLRequestAbortOnEndJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestAbortOnEndJob::StartAsync,
                                weak_factory_.GetWeakPtr()));
}

int URLRequestAbortOnEndJob::ReadRawData(net::IOBuffer* buf, int max_bytes) {
  if (!sent_data_) {
    max_bytes =
        std::min(max_bytes, base::checked_cast<int>(sizeof(kPageContent)));
    std::memcpy(buf->data(), kPageContent, max_bytes);
    sent_data_ = true;
    return max_bytes;
  }

  return net::ERR_CONNECTION_ABORTED;
}

}  // namespace content
