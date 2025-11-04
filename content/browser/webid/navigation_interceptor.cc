// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/navigation_interceptor.h"

#include "base/logging.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"

namespace content::webid {

// static
void NavigationInterceptor::MaybeCreateAndAdd(
    NavigationThrottleRegistry& registry) {
  if (!IsNavigationInterceptionEnabled()) {
    return;
  }
  registry.AddThrottle(std::make_unique<NavigationInterceptor>(registry));
}

NavigationInterceptor::NavigationInterceptor(
    NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

NavigationInterceptor::~NavigationInterceptor() = default;

content::NavigationThrottle::ThrottleCheckResult
NavigationInterceptor::WillProcessResponse() {
  // TODO(crbug.com/455614294): Implement navigation throttling logic.
  return PROCEED;
}

const char* NavigationInterceptor::GetNameForLogging() {
  return "FedCMNavigationInterceptor";
}

}  // namespace content::webid
