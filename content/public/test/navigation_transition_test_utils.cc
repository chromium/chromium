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

ScopedScreenshotCapturedObserverForTesting::
    ScopedScreenshotCapturedObserverForTesting(int expected_nav_entry_index) {
  NavigationTransitionTestUtils::SetNavScreenshotCallbackForTesting(
      base::BindRepeating(
          [](base::RepeatingClosure callback, int expected_nav_entry_index,
             int nav_entry_index, const SkBitmap& bitmap, bool requested) {
            CHECK_EQ(nav_entry_index, expected_nav_entry_index);
            CHECK(requested);
            std::move(callback).Run();
          },
          run_loop_.QuitClosure(), expected_nav_entry_index));
}

ScopedScreenshotCapturedObserverForTesting::
    ~ScopedScreenshotCapturedObserverForTesting() {
  // Reset the RepeatingCallback to a no-op.
  NavigationTransitionUtils::SetNavScreenshotCallbackForTesting(
      base::BindRepeating(
          [](int nav_entry_dex, const SkBitmap& bitmap, bool requested) {}));
}

void ScopedScreenshotCapturedObserverForTesting::Wait() {
  run_loop_.Run();
}

}  // namespace content
