// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_UTILS_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Size;
}

namespace content {

class NavigationRequest;

struct NavigationTransitionUtils {
  // See ScreenshotCallback in NavigationTransitionTestUtils.
  using ScreenshotCallback = base::RepeatingCallback<
      void(int nav_entry_index, const SkBitmap& bitmap, bool requested)>;

  // Capture the `NavigationEntryScreenshot` for the old page, and store the
  // screenshot in the old page's NavigationEntry.
  // Should only be called immediately before the old page is unloaded.
  static void CaptureNavigationEntryScreenshot(
      const NavigationRequest& navigation_request);

  // Used by tests to deterministically validate the memory budgeting / eviction
  // logic.
  CONTENT_EXPORT static void SetCapturedScreenshotSizeForTesting(
      const gfx::Size& size);

  // Returns the global counter of issued `viz::CopyOutputRequest`s.
  CONTENT_EXPORT static int GetNumCopyOutputRequestIssuedForTesting();

  // Resets the above counter to zero.
  CONTENT_EXPORT static void ResetNumCopyOutputRequestIssuedForTesting();

  // Calls `screenshot_callback` with the index of the previous NavigationEntry
  // when leaving a page, along with the generated bitmap captured by the
  // CaptureNavigationEntryScreenshot function.
  CONTENT_EXPORT static void SetNavScreenshotCallbackForTesting(
      ScreenshotCallback screenshot_callback);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_UTILS_H_
