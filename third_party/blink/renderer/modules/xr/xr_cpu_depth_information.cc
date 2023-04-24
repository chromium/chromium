// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_cpu_depth_information.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "base/numerics/checked_math.h"
#include "base/numerics/ostream_operators.h"
#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/geometry/point3_f.h"

namespace {
constexpr char kOutOfBoundsAccess[] =
    "Attempted to access data that is out-of-bounds.";

constexpr char kArrayBufferDetached[] =
    "Attempted to access data from a detached data buffer.";

size_t GetBytesPerElement(device::mojom::XRDepthDataFormat data_format) {
  switch (data_format) {
    case device::mojom::XRDepthDataFormat::kLuminanceAlpha:
      return 2;
    case device::mojom::XRDepthDataFormat::kFloat32:
      return 4;
  }
}
}  // namespace

namespace blink {

XRCPUDepthInformation::XRCPUDepthInformation(
    const XRFrame* xr_frame,
    const gfx::Size& size,
    const gfx::Transform& norm_texture_from_norm_view,
    float raw_value_to_meters,
    device::mojom::XRDepthDataFormat data_format,
    DOMArrayBuffer* data)
    : XRDepthInformation(xr_frame,
                         size,
                         norm_texture_from_norm_view,
                         raw_value_to_meters),
      data_(data),
      data_format_(data_format),
      bytes_per_element_(GetBytesPerElement(data_format)) {
  DVLOG(3) << __func__;

  CHECK_EQ(base::CheckMul(bytes_per_element_, size_.width(), size_.height())
               .ValueOrDie(),
           data_->ByteLength());
}

DOMArrayBuffer* XRCPUDepthInformation::data(
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

  // Check if `data_` is detached:
  if(data_->IsDetached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kArrayBufferDetached);
    return 0.0;
  }

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

  gfx::PointF norm_view_coordinates(x, y);

  gfx::PointF norm_depth_coordinates =
      norm_depth_buffer_from_norm_view_.MapPoint(norm_view_coordinates);

  gfx::PointF depth_coordinates =
      gfx::ScalePoint(norm_depth_coordinates, size_.width(), size_.height());

  uint32_t column = std::clamp<uint32_t>(
      static_cast<uint32_t>(depth_coordinates.x()), 0, size_.width() - 1);
  uint32_t row = std::clamp<uint32_t>(
      static_cast<uint32_t>(depth_coordinates.y()), 0, size_.height() - 1);

  auto checked_index =
      base::CheckAdd(column, base::CheckMul(row, size_.width()));
  size_t index = checked_index.ValueOrDie();

  // Convert from data's native units to meters when accessing:
  float result = GetItem(index) * raw_value_to_meters_;

  DVLOG(3) << __func__ << ": x=" << x << ", y=" << y << ", column=" << column
           << ", row=" << row << ", index=" << index << ", result=" << result;

  return result;
}

float XRCPUDepthInformation::GetItem(size_t index) const {
  DVLOG(3) << __func__ << ": index=" << index;

  CHECK(!data_->IsDetached());

  switch (data_format_) {
    case device::mojom::XRDepthDataFormat::kLuminanceAlpha: {
      // Luminance-alpha is 2 bytes per entry & base::make_span expects the
      // length to be provided in the number of elements. The constructor
      // enforces that |data_|'s byte length matches the size of the array,
      // taking into account the number of bytes per element.
      base::span<const uint16_t> array =
          base::make_span(reinterpret_cast<const uint16_t*>(data_->Data()),
                          data_->ByteLength() / bytes_per_element_);
      return array[index];
    }
    case device::mojom::XRDepthDataFormat::kFloat32: {
      // Float32 is 4 bytes per entry & base::make_span expects the length to be
      // provided in the number of elements. The constructor enforces that
      // |data_|'s byte length matches the size of the array, taking into
      // account the number of bytes per element.
      base::span<const float> array =
          base::make_span(reinterpret_cast<const float*>(data_->Data()),
                          data_->ByteLength() / bytes_per_element_);
      return array[index];
    }
  }
}

void XRCPUDepthInformation::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
  XRDepthInformation::Trace(visitor);
}

}  // namespace blink
