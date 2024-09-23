// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_UTILS_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace gfx {
class Size;
}

namespace content {

class NavigationRequest;

class NavigationTransitionUtils {
 public:
  NavigationTransitionUtils() = delete;

  // See ScreenshotCallback in NavigationTransitionTestUtils.
  using ScreenshotCallback =
      base::RepeatingCallback<void(int nav_entry_index,
                                   const SkBitmap& bitmap,
                                   bool requested,
                                   SkBitmap& out_override)>;

  // Capture the `NavigationEntryScreenshot` for the old page, and store the
  // screenshot in the old page's NavigationEntry.
  //
  // This is invoked at 2 points in the navigation's lifecycle, the screenshot is done at one
  // of these 2 points:
  //
  // 1. When dispatching a commit message from the browser to the renderer
  //    process.
  // 2. When the browser receives the DidCommitNavigation ack and the navigation
  //    is committed in the browser process.
  //
  // Returns true if a screenshot for the currently committed Document is
  // requested for this navigation.
  static bool CaptureNavigationEntryScreenshotForCrossDocumentNavigations(
      NavigationRequest& navigation_request,
      bool did_receive_commit_ack);

  // Called when `DidCommitSameDocumentNavigation` arrives at the browser, and
  // *before* the navigation commits. Ensures that a `NavigationEntryScreenshot`
  // for the pre-navigation DOM state is cached when provided by the Viz
  // process.
  static void SetSameDocumentNavigationEntryScreenshotToken(
      NavigationRequest& navigation_request,
      std::optional<blink::SameDocNavigationScreenshotDestinationToken>
          destination_token);

  // Used by tests to deterministically validate the memory budgeting / eviction
  // logic.
  CONTENT_EXPORT static void SetCapturedScreenshotSizeForTesting(
      const gfx::Size& size);

  // Returns the global counter of issued `viz::CopyOutputRequest`s.
  CONTENT_EXPORT static int GetNumCopyOutputRequestIssuedForTesting();

  // Resets the above counter to zero.
  CONTENT_EXPORT static void ResetNumCopyOutputRequestIssuedForTesting();

  // Calls `screenshot_callback` with the index of the previous NavigationEntry
  // when leaving a page, along with the generated bitmap captured captured for
  // all navigations.
  CONTENT_EXPORT static void SetNavScreenshotCallbackForTesting(
      ScreenshotCallback screenshot_callback);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_NAVIGATION_TRANSITIONS_NAVIGATION_TRANSITION_UTILS_H_
