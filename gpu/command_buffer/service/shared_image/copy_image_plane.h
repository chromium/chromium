// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_COPY_IMAGE_PLANE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_COPY_IMAGE_PLANE_H_

#include <cstdint>

#include "gpu/gpu_gles2_export.h"

namespace gpu {

// Copies a single plane of image data from `src_data` to `dst_data`.
// `row_width_bytes` will be copied for `height` rows.
// NOTE: `src_stride_bytes` and `dst_stride_bytes` must be >= `row_width_bytes`.
// Any data past `row_width_bytes` in a row will not be copied from or to.
GPU_GLES2_EXPORT void CopyImagePlane(const uint8_t* src_data,
                                     int src_stride_bytes,
                                     uint8_t* dst_data,
                                     int dst_stride_bytes,
                                     int row_width_bytes,
                                     int height);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_COPY_IMAGE_PLANE_H_
