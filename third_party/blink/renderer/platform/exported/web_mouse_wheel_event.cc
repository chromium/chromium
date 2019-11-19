// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_mouse_wheel_event.h"

namespace blink {

float WebMouseWheelEvent::DeltaXInRootFrame() const {
  return delta_x / frame_scale_;
}

float WebMouseWheelEvent::DeltaYInRootFrame() const {
  return delta_y / frame_scale_;
}

WebMouseWheelEvent WebMouseWheelEvent::FlattenTransform() const {
  WebMouseWheelEvent result = *this;
  result.delta_x /= result.frame_scale_;
  result.delta_y /= result.frame_scale_;
  result.FlattenTransformSelf();
  return result;
}

}  // namespace blink
