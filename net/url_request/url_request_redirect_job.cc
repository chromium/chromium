// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_redirect_job.h"

#include <string>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "net/base/features.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_errors.h"
#include "net/base/task/task_runner.h"
#include "net/http/http_log_util.h"
#include "net/http/http_raw_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_with_source.h"
#include "net/url_request/redirect_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace net {

namespace {
const scoped_refptr<base::SingleThreadTaskRunner>& TaskRunner(
    net::RequestPriority priority) {
  if (features::kNetTaskSchedulerURLRequestRedirectJob.Get()) {
    return net::GetTaskRunner(priority);
  }
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}
}  // namespace

URLRequestRedirectJob::URLRequestRedirectJob(
    URLRequest* request,
    const GURL& redirect_destination,
    RedirectUtil::ResponseCode response_code,
    const std::string& redirect_reason)
    : URLRequestJob(request),
      redirect_destination_(redirect_destination),
      response_code_(response_code),
      redirect_reason_(redirect_reason) {
  DCHECK(!redirect_reason_.empty());
}

URLRequestRedirectJob::~URLRequestRedirectJob() = default;

void URLRequestRedirectJob::GetResponseInfo(HttpResponseInfo* info) {
  // Should only be called after the URLRequest has been notified there's header
  // information.
  DCHECK(fake_headers_.get());

  // This assumes |info| is a freshly constructed HttpResponseInfo.
  info->headers = fake_headers_;
  info->request_time = response_time_;
  info->response_time = response_time_;
  info->original_response_time = response_time_;
}

void URLRequestRedirectJob::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  // Set send_start, send_end, and receive_headers_start to
  // receive_headers_end_ to be consistent with network cache behavior.
  load_timing_info->send_start = receive_headers_end_;
  load_timing_info->send_end = receive_headers_end_;
  load_timing_info->receive_headers_start = receive_headers_end_;
  load_timing_info->receive_headers_end = receive_headers_end_;
}

void URLRequestRedirectJob::Start() {
  request()->net_log().AddEventWithStringParams(
      NetLogEventType::URL_REQUEST_REDIRECT_JOB, "reason", redirect_reason_);
  TaskRunner(request_->priority())
      ->PostTask(FROM_HERE, base::BindOnce(&URLRequestRedirectJob::StartAsync,
                                           weak_factory_.GetWeakPtr()));
}

void URLRequestRedirectJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  URLRequestJob::Kill();
}

bool URLRequestRedirectJob::CopyFragmentOnRedirect(const GURL& location) const {
  // The instantiators have full control over the desired redirection target,
  // including the reference fragment part of the URL.
  return false;
}

void URLRequestRedirectJob::StartAsync() {
  DCHECK(request_);

  receive_headers_end_ = base::TimeTicks::Now();
  response_time_ = base::Time::Now();

  const HttpRequestHeaders& request_headers = request_->extra_request_headers();
  fake_headers_ = RedirectUtil::SynthesizeRedirectHeaders(
      redirect_destination_, response_code_, redirect_reason_, request_headers);

  NetLogResponseHeaders(
      request()->net_log(),
      NetLogEventType::URL_REQUEST_FAKE_RESPONSE_HEADERS_CREATED,
      fake_headers_.get());

  // TODO(mmenke):  Consider calling the NetworkDelegate with the headers here.
  // There's some weirdness about how to handle the case in which the delegate
  // tries to modify the redirect location, in terms of how IsSafeRedirect
  // should behave, and whether the fragment should be copied.
  URLRequestJob::NotifyHeadersComplete();
}

}  // namespace net
