// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/navigation_transition_test_utils.h"

#include "base/test/test_future.h"
#include "content/browser/renderer_host/navigation_controller_impl.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_entry_screenshot_cache.h"
#include "content/browser/renderer_host/navigation_transitions/navigation_transition_utils.h"

namespace content {

void NavigationTransitionTestUtils::SetNavScreenshotCallbackForTesting(
    ScreenshotCallback screenshot_callback) {
  NavigationTransitionUtils::SetNavScreenshotCallbackForTesting(
      std::move(screenshot_callback));
}

size_t NavigationTransitionTestUtils::WaitForScreenshotCompressed(
    NavigationController& controller,
    int nav_entry_index) {
  auto* screenshot = static_cast<NavigationEntryScreenshot*>(
      static_cast<NavigationControllerImpl&>(controller)
          .GetEntryAtIndex(nav_entry_index)
          ->GetUserData(NavigationEntryScreenshot::kUserDataKey));

  size_t compressed_size = screenshot->CompressedSizeForTesting();
  if (!compressed_size) {
    base::test::TestFuture<int> done;
    NavigationEntryScreenshotCache::SetCompressedCallbackForTesting(
        done.GetCallback());
    EXPECT_EQ(nav_entry_index, done.Get());
    compressed_size = screenshot->CompressedSizeForTesting();
  }

  return compressed_size;
}

ScopedScreenshotCapturedObserverForTesting::
    ScopedScreenshotCapturedObserverForTesting(int expected_nav_entry_index) {
  NavigationTransitionTestUtils::SetNavScreenshotCallbackForTesting(
      base::BindRepeating(
          [](base::RepeatingClosure callback, int expected_nav_entry_index,
             int nav_entry_index, const SkBitmap& bitmap, bool requested,
             SkBitmap& out_override) {
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
      base::BindRepeating([](int nav_entry_dex, const SkBitmap& bitmap,
                             bool requested, SkBitmap& out_override) {}));
}

void ScopedScreenshotCapturedObserverForTesting::Wait() {
  run_loop_.Run();
}

}  // namespace content
