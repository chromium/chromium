// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/gamepad/gamepad_touch.h"

namespace blink {

namespace {

DOMFloat32Array* ToFloat32Array(const double x, const double y) {
  DOMFloat32Array* out = DOMFloat32Array::Create(2);
  out->Data()[0] = x;
  out->Data()[1] = y;
  return out;
}

DOMUint32Array* ToUint32Array(const uint32_t width, const uint32_t height) {
  DOMUint32Array* out = DOMUint32Array::Create(2);
  out->Data()[0] = width;
  out->Data()[1] = height;
  return out;
}

}  // namespace

void GamepadTouch::SetPosition(float x, float y) {
  position_ = ToFloat32Array(x, y);
}

void GamepadTouch::SetSurfaceDimensions(uint32_t x, uint32_t y) {
  if (!surface_dimensions_) {
    surface_dimensions_ = ToUint32Array(x, y);
  }
  has_surface_dimensions_ = true;
}

bool GamepadTouch::IsEqual(const device::GamepadTouch& device_touch) const {
  return device_touch.touch_id == touch_id_ &&
         device_touch.surface_id == surface_id_ &&
         device_touch.has_surface_dimensions == has_surface_dimensions_ &&
         device_touch.x == position_->Data()[0] &&
         device_touch.y == position_->Data()[1] &&
         device_touch.surface_width == surface_dimensions_->Data()[0] &&
         device_touch.surface_height == surface_dimensions_->Data()[1];
}

void GamepadTouch::UpdateValuesFrom(const device::GamepadTouch& device_touch,
                                    uint32_t id_offset) {
  touch_id_ = id_offset;
  surface_id_ = device_touch.surface_id;
  position_ = ToFloat32Array(device_touch.x, device_touch.y);
  surface_dimensions_ =
      ToUint32Array(device_touch.surface_width, device_touch.surface_height);
  has_surface_dimensions_ = device_touch.has_surface_dimensions;
}

void GamepadTouch::Trace(Visitor* visitor) const {
  visitor->Trace(position_);
  visitor->Trace(surface_dimensions_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
