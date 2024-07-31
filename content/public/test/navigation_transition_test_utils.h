// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NAVIGATION_TRANSITION_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_NAVIGATION_TRANSITION_TEST_UTILS_H_

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {
class NavigationController;

// If set in `SetNavScreenshotCallbackForTesting`, this callback is invoked
// for each committed navigation when kBackForwardTransitions is enabled.
// nav_entry_index: Index for the entry the user is navigating away from.
// bitmap: The screenshot for the entry the
// user is navigating away from. This can be empty if no screenshot request
// was made or the request failed.
// requested: Whether a screenshot request was made or the navigation was
// deemed not eligible for screenshotting. The `bitmap` will always be
// empty if `requested` is false.
// out_override: Tests may provide an override bitmap to use. This is primarily
// used to ensure a capture bitmap is available in cases where certain required
// GPU features may not be available (e.g. in the emulator).
using ScreenshotCallback =
    base::RepeatingCallback<void(int nav_entry_index,
                                 const SkBitmap& bitmap,
                                 bool requested,
                                 SkBitmap& out_override)>;

struct NavigationTransitionTestUtils {
  // Calls `screenshot_callback` with the index of the previous NavigationEntry
  // when leaving a page, along with the generated bitmap captured by the
  // CaptureNavigationEntryScreenshot function.
  static void SetNavScreenshotCallbackForTesting(
      ScreenshotCallback screenshot_callback);

  // Waits for the compressed screenshot and returns its size in bytes.
  static size_t WaitForScreenshotCompressed(NavigationController& controller,
                                            int nav_entry_index);
};

// Wraps `SetNavScreenshotCallbackForTesting()`, so that the test doesn't have
// to update the `ScreenshotCallback` on every follow up navigations. Useful
// when the follow up navigations no longer care about the screenshot capture.
class ScopedScreenshotCapturedObserverForTesting {
 public:
  explicit ScopedScreenshotCapturedObserverForTesting(
      int expected_nav_entry_index);
  ScopedScreenshotCapturedObserverForTesting(
      const ScopedScreenshotCapturedObserverForTesting&) = delete;
  ScopedScreenshotCapturedObserverForTesting& operator=(
      const ScopedScreenshotCapturedObserverForTesting&) = delete;
  ~ScopedScreenshotCapturedObserverForTesting();

  // Blocks the execution until a screenshot is deposited into the navigation
  // entry at `expected_nav_entry_index`.
  void Wait();

 private:
  base::RunLoop run_loop_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NAVIGATION_TRANSITION_TEST_UTILS_H_
