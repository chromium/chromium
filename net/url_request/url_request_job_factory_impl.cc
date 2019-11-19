// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory_impl.h"

#include "base/stl_util.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_job_manager.h"
#include "url/gurl.h"

namespace net {

namespace {

URLRequestInterceptor* g_interceptor_for_testing = nullptr;

}  // namespace

URLRequestJobFactoryImpl::URLRequestJobFactoryImpl() = default;

URLRequestJobFactoryImpl::~URLRequestJobFactoryImpl() = default;

bool URLRequestJobFactoryImpl::SetProtocolHandler(
    const std::string& scheme,
    std::unique_ptr<ProtocolHandler> protocol_handler) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!protocol_handler) {
    auto it = protocol_handler_map_.find(scheme);
    if (it == protocol_handler_map_.end())
      return false;

    protocol_handler_map_.erase(it);
    return true;
  }

  if (base::Contains(protocol_handler_map_, scheme))
    return false;
  protocol_handler_map_[scheme] = std::move(protocol_handler);
  return true;
}

URLRequestJob* URLRequestJobFactoryImpl::MaybeCreateJobWithProtocolHandler(
    const std::string& scheme,
    URLRequest* request,
    NetworkDelegate* network_delegate) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (g_interceptor_for_testing) {
    URLRequestJob* job = g_interceptor_for_testing->MaybeInterceptRequest(
        request, network_delegate);
    if (job)
      return job;
  }

  auto it = protocol_handler_map_.find(scheme);
  if (it == protocol_handler_map_.end())
    return nullptr;
  return it->second->MaybeCreateJob(request, network_delegate);
}

bool URLRequestJobFactoryImpl::IsHandledProtocol(
    const std::string& scheme) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return base::Contains(protocol_handler_map_, scheme) ||
         URLRequestJobManager::GetInstance()->SupportsScheme(scheme);
}

bool URLRequestJobFactoryImpl::IsSafeRedirectTarget(
    const GURL& location) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!location.is_valid()) {
    // Error cases are safely handled.
    return true;
  }
  auto it = protocol_handler_map_.find(location.scheme());
  if (it == protocol_handler_map_.end()) {
    // Unhandled cases are safely handled.
    return true;
  }
  return it->second->IsSafeRedirectTarget(location);
}

// static
void URLRequestJobFactoryImpl::SetInterceptorForTesting(
    URLRequestInterceptor* interceptor) {
  DCHECK(!interceptor || !g_interceptor_for_testing);

  g_interceptor_for_testing = interceptor;
}

}  // namespace net
