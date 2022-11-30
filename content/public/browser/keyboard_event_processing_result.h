// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_KEYBOARD_EVENT_PROCESSING_RESULT_H_
#define CONTENT_PUBLIC_BROWSER_KEYBOARD_EVENT_PROCESSING_RESULT_H_

namespace content {

enum class KeyboardEventProcessingResult {
  // The event was handled.
  HANDLED,

#if defined(USE_AURA)
  // The event was handled, but don't update the underlying event. A value
  // HANDLED results in calling ui::Event::SetHandled(), where as this does not.
  HANDLED_DONT_UPDATE_EVENT,
#endif

  // The event was not handled and should be forwarded to the renderer.
  NOT_HANDLED,

  // The event was not handled and should be forwarded to the renderer.
  // Additionally the KeyEvent corresponds to a shortcut (aka accelerator).
  NOT_HANDLED_IS_SHORTCUT,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_KEYBOARD_EVENT_PROCESSING_RESULT_H_
