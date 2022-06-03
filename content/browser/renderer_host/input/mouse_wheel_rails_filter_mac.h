// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_MOUSE_WHEEL_RAILS_FILTER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_MOUSE_WHEEL_RAILS_FILTER_MAC_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace content {

class CONTENT_EXPORT MouseWheelRailsFilterMac {
 public:
  MouseWheelRailsFilterMac();
  ~MouseWheelRailsFilterMac();
  blink::WebInputEvent::RailsMode UpdateRailsMode(
      const blink::WebMouseWheelEvent& event);

 private:
  gfx::Vector2dF decayed_delta_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_MOUSE_WHEEL_RAILS_FILTER_MAC_H_
