// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DEVICE_EMULATION_PARAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_DEVICE_EMULATION_PARAMS_H_

#include "base/optional.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/public/platform/web_float_point.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_size.h"

namespace blink {

// All sizes are measured in device independent pixels.
struct WebDeviceEmulationParams {
  enum ScreenPosition { kDesktop, kMobile, kScreenPositionLast = kMobile };

  ScreenPosition screen_position;

  // Emulated screen size. Typically full / physical size of the device screen
  // in DIP. Empty size means using default value: original one for kDesktop
  // screen position, equal to |view_size| for kMobile.
  WebSize screen_size;

  // Position of view on the screen. Missing position means using default value:
  // original one for kDesktop screen position, (0, 0) for kMobile.
  base::Optional<WebPoint> view_position;

  // Emulated view size. A width or height of 0 means no override in that
  // dimension, but the other can still be applied. When both are 0, then the
  // |scale| will be applied to the view instead.
  WebSize view_size;

  // If zero, the original device scale factor is preserved.
  float device_scale_factor;

  // Scale the contents of the main frame. The view's size will be scaled by
  // this number when they are not specified in |view_size|.
  float scale;

  // Forced viewport offset for screenshots during emulation, (-1, -1) for
  // disabled.
  WebFloatPoint viewport_offset;

  // Viewport scale for screenshots during emulation, 0 for current.
  float viewport_scale;

  // Optional screen orientation type, with WebScreenOrientationUndefined
  // value meaning no emulation necessary.
  WebScreenOrientationType screen_orientation_type;

  // Screen orientation angle, used together with screenOrientationType.
  int screen_orientation_angle;

  WebDeviceEmulationParams()
      : screen_position(kDesktop),
        device_scale_factor(0),
        scale(1),
        viewport_offset(-1, -1),
        viewport_scale(0),
        screen_orientation_type(kWebScreenOrientationUndefined),
        screen_orientation_angle(0) {}
};

inline bool operator==(const WebDeviceEmulationParams& a,
                       const WebDeviceEmulationParams& b) {
  return a.screen_position == b.screen_position &&
         a.screen_size == b.screen_size && a.view_position == b.view_position &&
         a.device_scale_factor == b.device_scale_factor &&
         a.view_size == b.view_size && a.scale == b.scale &&
         a.screen_orientation_type == b.screen_orientation_type &&
         a.screen_orientation_angle == b.screen_orientation_angle &&
         a.viewport_offset == b.viewport_offset &&
         a.viewport_scale == b.viewport_scale;
}

inline bool operator!=(const WebDeviceEmulationParams& a,
                       const WebDeviceEmulationParams& b) {
  return !(a == b);
}

}  // namespace blink

#endif
