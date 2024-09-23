// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/copy_image_plane.h"

#include "base/check.h"
#include "base/check_op.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace gpu {

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

}  // namespace gpu
