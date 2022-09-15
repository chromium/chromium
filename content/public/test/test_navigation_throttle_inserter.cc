// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_navigation_throttle_inserter.h"

#include <utility>

#include "content/public/browser/navigation_handle.h"

namespace content {

TestNavigationThrottleInserter::TestNavigationThrottleInserter(
    WebContents* web_contents,
    ThrottleInsertionCallback callback)
    : WebContentsObserver(web_contents), callback_(std::move(callback)) {}

TestNavigationThrottleInserter::~TestNavigationThrottleInserter() = default;

void TestNavigationThrottleInserter::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  if (std::unique_ptr<NavigationThrottle> throttle =
          callback_.Run(navigation_handle)) {
    navigation_handle->RegisterThrottleForTesting(std::move(throttle));
  }
}

}  // namespace content
