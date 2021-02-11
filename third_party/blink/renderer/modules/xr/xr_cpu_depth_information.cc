// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_cpu_depth_information.h"

#include <cmath>
#include <cstdlib>

#include "base/numerics/checked_math.h"
#include "base/numerics/ranges.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/geometry/point3_f.h"

namespace {
constexpr char kOutOfBoundsAccess[] =
    "Attempted to access data that is out-of-bounds.";
}

namespace blink {

XRCPUDepthInformation::XRCPUDepthInformation(
    const XRFrame* xr_frame,
    const gfx::Size& size,
    const gfx::Transform& norm_texture_from_norm_view,
    float raw_value_to_meters,
    DOMUint16Array* data)
    : XRDepthInformation(xr_frame,
                         size,
                         norm_texture_from_norm_view,
                         raw_value_to_meters),
      data_(data) {
  DVLOG(3) << __func__;

  CHECK_EQ(base::CheckMul(2, size_.width(), size_.height()).ValueOrDie(),
           data_->byteLength());
}

DOMUint16Array* XRCPUDepthInformation::data(
    ExceptionState& exception_state) const {
  if (!ValidateFrame(exception_state)) {
    return nullptr;
  }

  return data_;
}

float XRCPUDepthInformation::getDepthInMeters(
    float x,
    float y,
    ExceptionState& exception_state) const {
  DVLOG(3) << __func__ << ": x=" << x << ", y=" << y;

  if (!ValidateFrame(exception_state)) {
    return 0.0;
  }

  if (x > 1.0 || x < 0.0) {
    exception_state.ThrowRangeError(kOutOfBoundsAccess);
    return 0.0;
  }

  if (y > 1.0 || y < 0.0) {
    exception_state.ThrowRangeError(kOutOfBoundsAccess);
    return 0.0;
  }

  // Those coordinates are actually `norm_view_coordinates` before a series of
  // transforms is applied, but they are modified in-place, so the name's in
  // anticipation of those transforms.
  gfx::Point3F depth_coordinates(x, y, 0.0);

  // `norm_view_coordinates` becomes `norm_depth_coordinates`:
  norm_depth_buffer_from_norm_view_.TransformPoint(&depth_coordinates);

  // `norm_depth_coordinates` becomes `depth_coordinates`:
  depth_coordinates.Scale(size_.width(), size_.height(), 1.0);

  uint32_t column = base::ClampToRange<uint32_t>(
      static_cast<uint32_t>(std::round(depth_coordinates.x())), 0,
      size_.width() - 1);
  uint32_t row = base::ClampToRange<uint32_t>(
      static_cast<uint32_t>(std::round(depth_coordinates.y())), 0,
      size_.height() - 1);

  auto checked_index =
      base::CheckAdd(column, base::CheckMul(row, size_.width()));
  size_t index = checked_index.ValueOrDie();

  // Convert from data's native units to meters when accessing:
  float result = data_->Item(index) * raw_value_to_meters_;

  DVLOG(3) << __func__ << ": x=" << x << ", y=" << y << ", column=" << column
           << ", row=" << row << ", index=" << index << ", result=" << result;

  return result;
}

void XRCPUDepthInformation::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
  XRDepthInformation::Trace(visitor);
}

}  // namespace blink
