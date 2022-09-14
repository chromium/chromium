// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_job_factory.h"

#include "base/containers/contains.h"
#include "net/base/net_errors.h"
#include "net/net_buildflags.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_interceptor.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace net {

namespace {

URLRequestInterceptor* g_interceptor_for_testing = nullptr;

// TODO(mmenke): Look into removing this class and
// URLRequestJobFactory::ProtocolHandlers completely. The only other subclass
// is iOS-only.
class HttpProtocolHandler : public URLRequestJobFactory::ProtocolHandler {
 public:
  // URLRequest::is_for_websockets() must match `is_for_websockets`, or requests
  // will be failed. This is so that attempts to fetch WebSockets requests
  // fails, and attempts to use HTTP URLs for WebSockets also fail.
  explicit HttpProtocolHandler(bool is_for_websockets)
      : is_for_websockets_(is_for_websockets) {}

  HttpProtocolHandler(const HttpProtocolHandler&) = delete;
  HttpProtocolHandler& operator=(const HttpProtocolHandler&) = delete;
  ~HttpProtocolHandler() override = default;

  std::unique_ptr<URLRequestJob> CreateJob(URLRequest* request) const override {
    if (request->is_for_websockets() != is_for_websockets_) {
      return std::make_unique<URLRequestErrorJob>(request,
                                                  ERR_UNKNOWN_URL_SCHEME);
    }
    return URLRequestHttpJob::Create(request);
  }

  const bool is_for_websockets_;
};

}  // namespace

URLRequestJobFactory::ProtocolHandler::~ProtocolHandler() = default;

bool URLRequestJobFactory::ProtocolHandler::IsSafeRedirectTarget(
    const GURL& location) const {
  return true;
}

URLRequestJobFactory::URLRequestJobFactory() {
  SetProtocolHandler(url::kHttpScheme, std::make_unique<HttpProtocolHandler>(
                                           /*is_for_websockets=*/false));
  SetProtocolHandler(url::kHttpsScheme, std::make_unique<HttpProtocolHandler>(
                                            /*is_for_websockets=*/false));
#if BUILDFLAG(ENABLE_WEBSOCKETS)
  SetProtocolHandler(url::kWsScheme, std::make_unique<HttpProtocolHandler>(
                                         /*is_for_websockets=*/true));
  SetProtocolHandler(url::kWssScheme, std::make_unique<HttpProtocolHandler>(
                                          /*is_for_websockets=*/true));
#endif  // BUILDFLAG(ENABLE_WEBSOCKETS)
}

URLRequestJobFactory::~URLRequestJobFactory() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool URLRequestJobFactory::SetProtocolHandler(
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

std::unique_ptr<URLRequestJob> URLRequestJobFactory::CreateJob(
    URLRequest* request) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // If we are given an invalid URL, then don't even try to inspect the scheme.
  if (!request->url().is_valid())
    return std::make_unique<URLRequestErrorJob>(request, ERR_INVALID_URL);

  if (g_interceptor_for_testing) {
    std::unique_ptr<URLRequestJob> job(
        g_interceptor_for_testing->MaybeInterceptRequest(request));
    if (job)
      return job;
  }

  auto it = protocol_handler_map_.find(request->url().scheme());
  if (it == protocol_handler_map_.end()) {
    return std::make_unique<URLRequestErrorJob>(request,
                                                ERR_UNKNOWN_URL_SCHEME);
  }

  return it->second->CreateJob(request);
}

bool URLRequestJobFactory::IsSafeRedirectTarget(const GURL& location) const {
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

void URLRequestJobFactory::SetInterceptorForTesting(
    URLRequestInterceptor* interceptor) {
  DCHECK(!interceptor || !g_interceptor_for_testing);

  g_interceptor_for_testing = interceptor;
}

}  // namespace net
