// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/navigation_throttle.h"

#include "content/browser/frame_host/navigation_request.h"

namespace content {

namespace {

net::Error DefaultNetErrorCode(NavigationThrottle::ThrottleAction action) {
  switch (action) {
    case NavigationThrottle::PROCEED:
    case NavigationThrottle::DEFER:
      return net::OK;
    case NavigationThrottle::CANCEL:
    case NavigationThrottle::CANCEL_AND_IGNORE:
      return net::ERR_ABORTED;
    case NavigationThrottle::BLOCK_REQUEST:
    case NavigationThrottle::BLOCK_REQUEST_AND_COLLAPSE:
      return net::ERR_BLOCKED_BY_CLIENT;
    case NavigationThrottle::BLOCK_RESPONSE:
      return net::ERR_BLOCKED_BY_RESPONSE;
    default:
      NOTREACHED();
      return net::ERR_UNEXPECTED;
  }
}

}  // namespace

NavigationThrottle::ThrottleCheckResult::ThrottleCheckResult(
    NavigationThrottle::ThrottleAction action)
    : NavigationThrottle::ThrottleCheckResult(action,
                                              DefaultNetErrorCode(action),
                                              base::nullopt) {}

NavigationThrottle::ThrottleCheckResult::ThrottleCheckResult(
    NavigationThrottle::ThrottleAction action,
    net::Error net_error_code)
    : NavigationThrottle::ThrottleCheckResult(action,
                                              net_error_code,
                                              base::nullopt) {}

NavigationThrottle::ThrottleCheckResult::ThrottleCheckResult(
    NavigationThrottle::ThrottleAction action,
    net::Error net_error_code,
    base::Optional<std::string> error_page_content)
    : action_(action),
      net_error_code_(net_error_code),
      error_page_content_(error_page_content) {}

NavigationThrottle::ThrottleCheckResult::ThrottleCheckResult(
    const ThrottleCheckResult& other) = default;

NavigationThrottle::ThrottleCheckResult::~ThrottleCheckResult() {}

NavigationThrottle::NavigationThrottle(NavigationHandle* navigation_handle)
    : navigation_handle_(navigation_handle) {}

NavigationThrottle::~NavigationThrottle() {}

NavigationThrottle::ThrottleCheckResult NavigationThrottle::WillStartRequest() {
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationThrottle::WillRedirectRequest() {
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult NavigationThrottle::WillFailRequest() {
  return NavigationThrottle::PROCEED;
}

NavigationThrottle::ThrottleCheckResult
NavigationThrottle::WillProcessResponse() {
  return NavigationThrottle::PROCEED;
}

void NavigationThrottle::Resume() {
  if (resume_callback_) {
    resume_callback_.Run();
    return;
  }
  NavigationRequest::From(navigation_handle_)->Resume(this);
}

void NavigationThrottle::CancelDeferredNavigation(
    NavigationThrottle::ThrottleCheckResult result) {
  if (cancel_deferred_navigation_callback_) {
    cancel_deferred_navigation_callback_.Run(result);
    return;
  }
  NavigationRequest::From(navigation_handle_)
      ->CancelDeferredNavigation(this, result);
}

}  // namespace content
