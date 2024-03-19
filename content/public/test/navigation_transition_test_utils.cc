// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/navigation_transition_test_utils.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"

namespace content {

void NavigationTransitionTestUtils::SetNavScreenshotCallbackForTesting(
    ScreenshotCallback screenshot_callback) {
  NavigationTransitionUtils::SetNavScreenshotCallbackForTesting(
      std::move(screenshot_callback));
}

}  // namespace content
