// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_cpu_depth_information.h"

#include <cstdlib>

#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

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
    uint32_t column,
    uint32_t row,
    ExceptionState& exception_state) const {
  DVLOG(3) << __func__ << ": column=" << column << ", row=" << row;

  if (!ValidateFrame(exception_state)) {
    return 0.0;
  }

  if (column >= static_cast<size_t>(size_.width())) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kOutOfBoundsAccess);
    return 0.0;
  }

  if (row >= static_cast<size_t>(size_.height())) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kOutOfBoundsAccess);
    return 0.0;
  }

  auto checked_index =
      base::CheckAdd(column, base::CheckMul(row, size_.width()));
  size_t index = checked_index.ValueOrDie();

  // Convert from data's native units to meters when accessing:
  float result = data_->Item(index) * rawValueToMeters_;

  DVLOG(3) << __func__ << ": index=" << index << ", result=" << result;

  return result;
}

void XRCPUDepthInformation::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
  XRDepthInformation::Trace(visitor);
}

}  // namespace blink
