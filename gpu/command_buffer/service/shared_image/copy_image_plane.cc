// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/copy_image_plane.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/numerics/checked_math.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace gpu {
namespace {

size_t ComputeBytesToCopy(int stride_bytes, int row_width_bytes, int height) {
  // Compute the number of byte as int since that is what libyuv::CopyPlane()
  // uses internally. Negative height is allowed but is a signal to flip and
  // the absolute value is used instead.
  int src_min_size =
      base::CheckAdd(
          base::CheckMul(stride_bytes, base::CheckSub(std::abs(height), 1)),
          row_width_bytes)
          .ValueOrDie();
  return base::checked_cast<size_t>(src_min_size);
}

}  // namespace

void CopyImagePlane(const uint8_t* src_data,
                    int src_stride_bytes,
                    uint8_t* dst_data,
                    int dst_stride_bytes,
                    int row_width_bytes,
                    int height) {
  CHECK(src_data);
  CHECK(dst_data);
  CHECK_GE(src_stride_bytes, row_width_bytes);
  CHECK_GE(dst_stride_bytes, row_width_bytes);

  libyuv::CopyPlane(src_data, src_stride_bytes, dst_data, dst_stride_bytes,
                    row_width_bytes, height);
}

void CopyImagePlane(base::span<const uint8_t> src_data,
                    int src_stride_bytes,
                    base::span<uint8_t> dst_data,
                    int dst_stride_bytes,
                    int row_width_bytes,
                    int height) {
  if (row_width_bytes <= 0 || height == 0) {
    // No bytes will be copied so skip validation too.
    return;
  }

  CHECK_GE(src_stride_bytes, row_width_bytes);
  CHECK_GE(dst_stride_bytes, row_width_bytes);

  // Since the libyuv isn't span aware, check the spans sizes are sufficient
  // before copying plane data.
  CHECK_GE(src_data.size(),
           ComputeBytesToCopy(src_stride_bytes, row_width_bytes, height));
  CHECK_GE(dst_data.size(),
           ComputeBytesToCopy(dst_stride_bytes, row_width_bytes, height));

  libyuv::CopyPlane(src_data.data(), src_stride_bytes, dst_data.data(),
                    dst_stride_bytes, row_width_bytes, height);
}

}  // namespace gpu
