// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NAVIGATION_THROTTLE_INSERTER_H_
#define CONTENT_PUBLIC_TEST_TEST_NAVIGATION_THROTTLE_INSERTER_H_

#include <memory>

#include "base/functional/callback.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class NavigationThrottle;
class WebContents;

using ThrottleInsertionCallback =
    base::RepeatingCallback<std::unique_ptr<NavigationThrottle>(
        NavigationHandle*)>;

// This class is instantiated with a NavigationThrottle factory callback, and
//  - Calls the callback in every DidStartNavigation.
//  - If the throttle is successfully created, registers it with the given
//    navigation.
class TestNavigationThrottleInserter : public WebContentsObserver {
 public:
  TestNavigationThrottleInserter(WebContents* web_contents,
                                 ThrottleInsertionCallback callback);

  TestNavigationThrottleInserter(const TestNavigationThrottleInserter&) =
      delete;
  TestNavigationThrottleInserter& operator=(
      const TestNavigationThrottleInserter&) = delete;

  ~TestNavigationThrottleInserter() override;

  // WebContentsObserver:
  void DidStartNavigation(NavigationHandle* navigation_handle) override;

 private:
  ThrottleInsertionCallback callback_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NAVIGATION_THROTTLE_INSERTER_H_
