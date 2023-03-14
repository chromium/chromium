// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/content_export.h"
#include "ui/display/types/display_constants.h"

#ifndef CONTENT_PUBLIC_BROWSER_FULLSCREEN_TYPES_H_
#define CONTENT_PUBLIC_BROWSER_FULLSCREEN_TYPES_H_

namespace content {

// Content fullscreen modes pertinent to windows that host web content.
enum class FullscreenMode {
  // Windowed content mode, i.e. the content has not invoked a fullscreen mode.
  // This currently includes browser window fullscreen modes, invoked via menus
  // or keyboard accelerators, without fullscreen JS APIs.
  kWindowed,
  // Content-fullscreen mode invoked via the Element.requestFullscreen() JS API.
  // The window is made fullscreen with content taking up the entire screen.
  // Also known as "HTML5 fullscreen" or "HTML element fullscreen".
  // See https://fullscreen.spec.whatwg.org
  kContent,
  // Pseudo content-fullscreen mode invoked while the content is being captured.
  // The window is not made fullscreen and content appears in the browser frame.
  // See FullscreenController's "FullscreenWithinTab Note".
  kPseudoContent,
};

// Fullscreen state information for windows that host web content.
struct CONTENT_EXPORT FullscreenState {
  // The target mode, updated before some async window state changes, i.e. when
  // the browser grants JS API requests to enter or exit fullscreen modes, or is
  // notified of window state changes invoked without fullscreen JS APIs.
  FullscreenMode target_mode = FullscreenMode::kWindowed;

  // The target display id, updated before some async window state changes, i.e.
  // when the browser grants JS API requests that target a specific display.
  // Valid when `target_mode` is kContent, otherwise it's `kInvalidDisplayId`.
  int64_t target_display_id = display::kInvalidDisplayId;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_FULLSCREEN_TYPES_H_
