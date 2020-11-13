// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/plane.h"

#include <string.h>

#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace blink {

namespace {

void CopyPlane(const uint8_t* plane,
               size_t copy_size,
               size_t trailing_zeros_size,
               uint8_t* dest) {
  // Copy plane bytes.
  memcpy(dest, plane, copy_size);

  // Zero trailing padding bytes.
  memset(dest + copy_size, 0, trailing_zeros_size);
}

}  // namespace

Plane::Plane(scoped_refptr<VideoFrameHandle> handle, size_t plane)
    : handle_(std::move(handle)), plane_(plane) {
#if DCHECK_IS_ON()
  // Validate the plane index, but only if the handle is valid.
  auto local_frame = handle_->frame();
  if (local_frame) {
    DCHECK(local_frame->IsMappable() || local_frame->HasGpuMemoryBuffer());
    DCHECK_LT(plane, local_frame->layout().num_planes());
  }
#endif  // DCHECK_IS_ON()
}

uint32_t Plane::stride() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  // TODO(sandersd): Consider returning row_bytes() instead. This would imply
  // removing padding bytes in copyInto().
  return local_frame->stride(plane_);
}

uint32_t Plane::rows() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;
  return local_frame->rows(plane_);
}

uint32_t Plane::length() const {
  auto local_frame = handle_->frame();
  if (!local_frame)
    return 0;

  // Note: this could be slightly larger than the actual data size. readInto()
  // will pad with zeros.
  return local_frame->rows(plane_) * local_frame->stride(plane_);
}

void Plane::readInto(MaybeShared<DOMArrayBufferView> dst,
                     ExceptionState& exception_state) {
  auto local_frame = handle_->frame();
  if (!local_frame) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Cannot read from destroyed VideoFrame.");
    return;
  }

  // Note: these methods all return int.
  size_t rows = local_frame->rows(plane_);
  size_t row_bytes = local_frame->row_bytes(plane_);
  size_t stride = local_frame->stride(plane_);

  DCHECK_GT(rows, 0u);       // should fail VideoFrame::IsValidConfig()
  DCHECK_GT(row_bytes, 0u);  // should fail VideoFrame::IsValidConfig()
  DCHECK_GE(stride, row_bytes);

  size_t total_size = rows * stride;
  size_t trailing_zeros_size = stride - row_bytes;
  size_t copy_size = total_size - trailing_zeros_size;

  // Note: byteLength is zero if the buffer is detached.
  DOMArrayBufferView* view = dst.View();
  uint8_t* base = static_cast<uint8_t*>(view->BaseAddressMaybeShared());
  if (!base) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Destination buffer is not valid.");
    return;
  }
  if (total_size > view->byteLength()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "Destination buffer is not large enough.");
    return;
  }

  if (local_frame->IsMappable()) {
    CopyPlane(local_frame->data(plane_), copy_size, trailing_zeros_size, base);
    return;
  }

  DCHECK(local_frame->HasGpuMemoryBuffer());
  auto* gmb = local_frame->GetGpuMemoryBuffer();
  if (!gmb->Map()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Could not map video frame into memory.");
    return;
  }
  CopyPlane(static_cast<const uint8_t*>(gmb->memory(plane_)), copy_size,
            trailing_zeros_size, base);
  gmb->Unmap();
}

}  // namespace blink
