// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_depth_information.h"

#include <cstdlib>

#include "base/numerics/checked_math.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/xr_frame.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace {
constexpr char kOutOfBoundsAccess[] =
    "Attempted to access data that is out-of-bounds.";
constexpr char kFrameInactive[] =
    "XRDepthInformation members are only accessible when their XRFrame's "
    "`active` boolean is `true`.";
constexpr char kFrameNotAnimated[] =
    "XRDepthInformation members are only accessible when their XRFrame's "
    "`animationFrame` boolean is `true`.";
}

namespace blink {

XRDepthInformation::XRDepthInformation(
    const XRFrame* xr_frame,
    const gfx::Size& size,
    const gfx::Transform& norm_texture_from_norm_view,
    DOMUint16Array* data)
    : xr_frame_(xr_frame),
      size_(size),
      data_(data),
      norm_texture_from_norm_view_(norm_texture_from_norm_view) {
  DVLOG(3) << __func__ << ": size_=" << size_.ToString()
           << ", norm_texture_from_norm_view_="
           << norm_texture_from_norm_view_.ToString();

  CHECK_EQ(base::CheckMul(2, size_.width(), size_.height()).ValueOrDie(),
           data_->byteLength());
}

DOMUint16Array* XRDepthInformation::data(
    ExceptionState& exception_state) const {
  if (!ValidateFrame(exception_state)) {
    return nullptr;
  }

  return data_;
}

uint32_t XRDepthInformation::width(ExceptionState& exception_state) const {
  if (!ValidateFrame(exception_state)) {
    return 0;
  }

  return size_.width();
}

uint32_t XRDepthInformation::height(ExceptionState& exception_state) const {
  if (!ValidateFrame(exception_state)) {
    return 0;
  }

  return size_.height();
}

float XRDepthInformation::getDepth(uint32_t column,
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

  // Data is stored in millimeters, convert to meters when accessing:
  float result = data_->Item(index) / 1000.0;

  DVLOG(3) << __func__ << ": index=" << index << ", result=" << result;

  return result;
}

XRRigidTransform* XRDepthInformation::normTextureFromNormView(
    ExceptionState& exception_state) const {
  if (!ValidateFrame(exception_state)) {
    return nullptr;
  }

  return MakeGarbageCollected<XRRigidTransform>(
      TransformationMatrix(norm_texture_from_norm_view_.matrix()));
}

bool XRDepthInformation::ValidateFrame(ExceptionState& exception_state) const {
  if (!xr_frame_->IsActive()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kFrameInactive);
    return false;
  }

  if (!xr_frame_->IsAnimationFrame()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kFrameNotAnimated);
    return false;
  }

  return true;
}

void XRDepthInformation::Trace(Visitor* visitor) const {
  visitor->Trace(xr_frame_);
  visitor->Trace(data_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
