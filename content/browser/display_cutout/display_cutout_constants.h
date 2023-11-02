// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DISPLAY_CUTOUT_DISPLAY_CUTOUT_CONSTANTS_H_
#define CONTENT_BROWSER_DISPLAY_CUTOUT_DISPLAY_CUTOUT_CONSTANTS_H_

namespace content {

// Contains the reasons why a |RenderFrameHost| does not have control over a
// the Display Cutout. This enum is used in metrics so the order should not
// be changed.
enum DisplayCutoutIgnoredReason {
  // The frame was not ignored.
  kAllowed = 0,

  // The frame was ignored because it was not the current active fullscreen
  // frame.
  kFrameNotCurrentFullscreen,

  // The frame was ignored because the WebContents was not fullscreen.
  kWebContentsNotFullscreen,
};

// Contains flags as to which safe areas are present and greater than zero. This
// enum is used in metrics so the order should not be changed.
enum DisplayCutoutSafeArea {
  kEmpty = 0,
  kTop = 1 << 0,
  kLeft = 1 << 1,
  kBottom = 1 << 2,
  kRight = 1 << 3,
};

}  // namespace content

#endif  // CONTENT_BROWSER_DISPLAY_CUTOUT_DISPLAY_CUTOUT_CONSTANTS_H_
