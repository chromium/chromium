// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_cpu_depth_information.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/ostream_operators.h"
#include "device/vr/public/mojom/xr_session.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/geometry/point3_f.h"

namespace {
constexpr char kOutOfBoundsAccess[] =
    "Attempted to access data that is out-of-bounds.";

constexpr char kArrayBufferDetached[] =
    "Attempted to access data from a detached data buffer.";

constexpr size_t GetBytesPerElement(
    device::mojom::XRDepthDataFormat data_format) {
  switch (data_format) {
    case device::mojom::XRDepthDataFormat::kLuminanceAlpha:
    case device::mojom::XRDepthDataFormat::kUnsignedShort:
      return 2;
    case device::mojom::XRDepthDataFormat::kFloat32:
      return 4;
  }
}

// We have to use the type names below, this enables us to ensure that we are
// using them properly in a switch statement.
static_assert(
    GetBytesPerElement(device::mojom::XRDepthDataFormat::kLuminanceAlpha) ==
    sizeof(uint16_t));
static_assert(
    GetBytesPerElement(device::mojom::XRDepthDataFormat::kUnsignedShort) ==
    sizeof(uint16_t));
static_assert(GetBytesPerElement(device::mojom::XRDepthDataFormat::kFloat32) ==
              sizeof(float));
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

  return data_.Get();
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

  // This generates a non-fixed span of size `bytes_per_element_`. We will need
  // to use the templated version of `first` below once we know the type to
  // generate a fixed span, which we unfortunately cannot do at this time.
  const auto offset = index * bytes_per_element_;
  auto value = data_->ByteSpan().subspan(offset).first(bytes_per_element_);

  switch (data_format_) {
    case device::mojom::XRDepthDataFormat::kUnsignedShort:
    case device::mojom::XRDepthDataFormat::kLuminanceAlpha: {
      // This should also be guaranteed by that static_asserts above.
      CHECK_EQ(bytes_per_element_, sizeof(uint16_t));
      return base::U16FromNativeEndian(value.first<sizeof(uint16_t)>());
    }
    case device::mojom::XRDepthDataFormat::kFloat32: {
      // This should also be guaranteed by that static_asserts above.
      CHECK_EQ(bytes_per_element_, sizeof(float));
      return base::FloatFromNativeEndian(value.first<sizeof(float)>());
    }
  }
}

void XRCPUDepthInformation::Trace(Visitor* visitor) const {
  visitor->Trace(data_);
  XRDepthInformation::Trace(visitor);
}

}  // namespace blink
