// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_throttle_inserter.h"

#include <utility>

#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

TestNavigationThrottleInserter::TestNavigationThrottleInserter(
    WebContents* web_contents,
    ThrottleInsertionCallback callback)
    : WebContentsObserver(web_contents), callback_(std::move(callback)) {}

TestNavigationThrottleInserter::TestNavigationThrottleInserter(
    WebContents* web_contents,
    NewThrottleInsertionCallback callback)
    : WebContentsObserver(web_contents), new_callback_(std::move(callback)) {}

TestNavigationThrottleInserter::~TestNavigationThrottleInserter() = default;

void TestNavigationThrottleInserter::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (callback_) {
    if (std::unique_ptr<NavigationThrottle> throttle =
            callback_.Run(navigation_handle)) {
      navigation_handle->RegisterThrottleForTesting(std::move(throttle));
    }
  }
  if (new_callback_) {
    new_callback_.Run(*NavigationRequest::From(navigation_handle)
                           ->GetNavigationThrottleRunnerForTesting());
  }
}

}  // namespace content
