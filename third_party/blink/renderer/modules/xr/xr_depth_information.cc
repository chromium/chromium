// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_depth_information.h"

#include <cstdlib>

#include "base/numerics/checked_math.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/xr/xr_rigid_transform.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace {
constexpr char kOutOfBoundsAccess[] =
    "Attempted to access data that is out-of-bounds.";
}

namespace blink {

XRDepthInformation::XRDepthInformation(
    const device::mojom::blink::XRDepthData& depth_data)
    : width_(depth_data.size.width()),
      height_(depth_data.size.height()),
      norm_texture_from_norm_view_(*depth_data.norm_texture_from_norm_view) {
  DVLOG(3) << __func__ << ": width_=" << width_ << ", height_=" << height_
           << ", norm_texture_from_norm_view_="
           << norm_texture_from_norm_view_.ToString();

  CHECK_EQ(base::CheckMul(2, width_, height_).ValueOrDie(),
           depth_data.pixel_data->size());

  base::span<const uint16_t> pixel_data = base::make_span(
      reinterpret_cast<const uint16_t*>(depth_data.pixel_data->data()),
      depth_data.pixel_data->size() / 2);

  // Copy the underlying pixel data into DOMUint16Array:
  data_ = DOMUint16Array::Create(pixel_data.data(), pixel_data.size());
}

DOMUint16Array* XRDepthInformation::data() const {
  return data_;
}

uint32_t XRDepthInformation::width() const {
  return width_;
}

uint32_t XRDepthInformation::height() const {
  return height_;
}

float XRDepthInformation::getDepth(uint32_t column,
                                   uint32_t row,
                                   ExceptionState& exception_state) const {
  DVLOG(3) << __func__ << ": column=" << column << ", row=" << row;

  if (column >= width_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kOutOfBoundsAccess);
    return 0.0;
  }

  if (row >= height_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotAllowedError,
                                      kOutOfBoundsAccess);
    return 0.0;
  }

  auto checked_index = base::CheckAdd(column, base::CheckMul(row, width_));
  size_t index = checked_index.ValueOrDie();

  // Data is stored in millimeters, convert to meters when accessing:
  float result = data_->Item(index) / 1000.0;

  DVLOG(3) << __func__ << ": index=" << index << ", result=" << result;

  return result;
}

XRRigidTransform* XRDepthInformation::normTextureFromNormView() const {
  return MakeGarbageCollected<XRRigidTransform>(
      TransformationMatrix(norm_texture_from_norm_view_.matrix()));
}

void XRDepthInformation::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
