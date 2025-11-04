// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_NAVIGATION_INTERCEPTOR_H_
#define CONTENT_BROWSER_WEBID_NAVIGATION_INTERCEPTOR_H_

#include "content/common/content_export.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {

class NavigationThrottleRegistry;

namespace webid {

// The NavigationInterceptor enables Identity Providers to control
// navigations to their endpoints by cancelling it and replacing it
// with an inline FedCM flow instead.
class CONTENT_EXPORT NavigationInterceptor
    : public content::NavigationThrottle {
 public:
  explicit NavigationInterceptor(NavigationThrottleRegistry& registry);
  ~NavigationInterceptor() override;

  NavigationInterceptor(const NavigationInterceptor&) = delete;
  NavigationInterceptor& operator=(const NavigationInterceptor&) = delete;

  // content::NavigationThrottle overrides:
  ThrottleCheckResult WillProcessResponse() override;
  const char* GetNameForLogging() override;

  static void MaybeCreateAndAdd(NavigationThrottleRegistry& registry);
};

}  // namespace webid
}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_NAVIGATION_INTERCEPTOR_H_
