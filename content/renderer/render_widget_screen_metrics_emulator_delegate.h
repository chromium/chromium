// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDER_WIDGET_SCREEN_METRICS_EMULATOR_DELEGATE_H_
#define CONTENT_RENDERER_RENDER_WIDGET_SCREEN_METRICS_EMULATOR_DELEGATE_H_

#include "content/common/content_export.h"

namespace blink {
struct WebDeviceEmulationParams;
}

namespace content {

// Consumers of RenderWidgetScreenMetricsEmulatorDelegate implement this
// delegate in order to transport emulation information across processes.
class CONTENT_EXPORT RenderWidgetScreenMetricsEmulatorDelegate {
 public:
  // Passes device emulation parameters to the delegate.
  virtual void SetScreenMetricsEmulationParameters(
      bool enabled,
      const blink::WebDeviceEmulationParams& params) = 0;

  // Passes an updated ScreenInfo and sizes to the delegate.
  virtual void SetScreenInfoAndSize(const ScreenInfo& screen_info,
                                    const gfx::Size& widget_size,
                                    const gfx::Size& visible_viewport_size) = 0;

  // Passes new view bounds and window bounds in screen coordinates to the
  // delegate.
  virtual void SetScreenRects(const gfx::Rect& view_screen_rect,
                              const gfx::Rect& window_screen_rect) = 0;

 protected:
  virtual ~RenderWidgetScreenMetricsEmulatorDelegate() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDER_WIDGET_SCREEN_METRICS_EMULATOR_DELEGATE_H_
