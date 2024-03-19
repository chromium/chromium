// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NAVIGATION_TRANSITION_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_NAVIGATION_TRANSITION_TEST_UTILS_H_

#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

// If set in `SetNavScreenshotCallbackForTesting`, this callback is invoked
// for each committed navigation when kBackForwardTransitions is enabled.
// nav_entry_index: Index for the entry the user is navigating away from.
// bitmap: The screenshot for the entry the
// user is navigating away from. This can be empty if no screenshot request
// was made or the request failed.
// requested: Whether a screenshot request was made or the navigation was
// deemed not eligible for screenshotting. The `bitmap` will always be
// empty if `requested` is false.
using ScreenshotCallback = base::RepeatingCallback<
    void(int nav_entry_index, const SkBitmap& bitmap, bool requested)>;

struct NavigationTransitionTestUtils {
  // Calls `screenshot_callback` with the index of the previous NavigationEntry
  // when leaving a page, along with the generated bitmap captured by the
  // CaptureNavigationEntryScreenshot function.
  static void SetNavScreenshotCallbackForTesting(
      ScreenshotCallback screenshot_callback);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NAVIGATION_TRANSITION_TEST_UTILS_H_
