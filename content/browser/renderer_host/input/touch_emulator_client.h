// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_CLIENT_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_CLIENT_H_

#include "content/common/content_export.h"
#include "content/common/cursors/webcursor.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "ui/base/ui_base_types.h"

namespace content {

class RenderWidgetHostViewBase;

// Emulates touch input with mouse and keyboard.
class CONTENT_EXPORT TouchEmulatorClient {
 public:
  virtual ~TouchEmulatorClient() {}

  virtual void ForwardEmulatedGestureEvent(
      const blink::WebGestureEvent& event) = 0;
  virtual void ForwardEmulatedTouchEvent(const blink::WebTouchEvent& event,
                                         RenderWidgetHostViewBase* target) = 0;
  virtual void SetCursor(const WebCursor& cursor) = 0;
  // |target| is the view associated with the corresponding input event.
  virtual void ShowContextMenuAtPoint(const gfx::Point& point,
                                      const ui::MenuSourceType source_type,
                                      RenderWidgetHostViewBase* target) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_EMULATOR_CLIENT_H_
