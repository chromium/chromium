// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_intercepting_job_factory.h"

#include <utility>

#include "base/logging.h"
#include "net/url_request/url_request_interceptor.h"

namespace net {

URLRequestInterceptingJobFactory::URLRequestInterceptingJobFactory(
    std::unique_ptr<URLRequestJobFactory> job_factory,
    std::unique_ptr<URLRequestInterceptor> interceptor)
    : owning_(true),
      job_factory_(job_factory.release()),
      interceptor_(interceptor.release()) {}

URLRequestInterceptingJobFactory::URLRequestInterceptingJobFactory(
    URLRequestJobFactory* job_factory,
    URLRequestInterceptor* interceptor)
    : owning_(false), job_factory_(job_factory), interceptor_(interceptor) {}

URLRequestInterceptingJobFactory::~URLRequestInterceptingJobFactory() {
  if (owning_) {
    delete job_factory_;
    delete interceptor_;
  }
}

URLRequestJob* URLRequestInterceptingJobFactory::
MaybeCreateJobWithProtocolHandler(
    const std::string& scheme,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  URLRequestJob* job = interceptor_->MaybeInterceptRequest(request,
                                                           network_delegate);
  if (job)
    return job;
  return job_factory_->MaybeCreateJobWithProtocolHandler(
      scheme, request, network_delegate);
}

bool URLRequestInterceptingJobFactory::IsHandledProtocol(
    const std::string& scheme) const {
  return job_factory_->IsHandledProtocol(scheme);
}

bool URLRequestInterceptingJobFactory::IsSafeRedirectTarget(
    const GURL& location) const {
  return job_factory_->IsSafeRedirectTarget(location);
}

}  // namespace net
