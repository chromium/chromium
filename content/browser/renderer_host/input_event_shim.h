// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_EVENT_SHIM_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_EVENT_SHIM_H_

#include "content/common/text_input_state.h"

namespace content {

// This exists to allow BrowserPluginGuest a safe way to intercept
// various input event messages normally handled by RenderWidgetHost.
//
// TODO(https://crbug.com/533069): Remove this class and all APIs using it
// when BrowserPlugin is fully deleted.
class InputEventShim {
 public:
  virtual ~InputEventShim() {}
  virtual void DidSetHasTouchEventHandlers(bool accept) = 0;
  virtual void DidTextInputStateChange(const TextInputState& params) = 0;
  virtual void DidLockMouse(bool user_gesture, bool privileged) = 0;
  virtual void DidUnlockMouse() = 0;
};

}  // namespace content

#endif /* CONTENT_BROWSER_RENDERER_HOST_INPUT_EVENT_SHIM_H_ */
