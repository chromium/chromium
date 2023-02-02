// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_REPACK_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_REPACK_UTILS_H_

#include <vector>

#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/geometry/size.h"

class SkPixmap;

namespace gpu {

// Repacks `src_pixmap` as 3 byte RGB pixel suitable for GL_RGB pixel upload.
// `src_pixmap` must either be 4 byte RGBX or BGRX. Repacked data will have
// 4 byte stride alignment.
GPU_GLES2_EXPORT std::vector<uint8_t> RepackPixelDataAsRgb(
    const gfx::Size& size,
    const SkPixmap& src_pixmap,
    bool src_is_bgrx);

// Repacks `src_pixmap` so it has `dst_stride`. `dst_stride` must be smaller
// than the stride of `src_pixmap`.
GPU_GLES2_EXPORT std::vector<uint8_t> RepackPixelDataWithStride(
    const gfx::Size& size,
    const SkPixmap& src_pixmap,
    size_t dst_stride);

// Unpacks `src_data` into `dst_pixmap`. The stride of `dst_pixmap` must be
// larger than `src_stride`.
GPU_GLES2_EXPORT void UnpackPixelDataWithStride(
    const gfx::Size& size,
    const std::vector<uint8_t>& src_data,
    size_t src_stride,
    const SkPixmap& dst_pixmap);

// Swizzles RGBA <-> BGRA / RGBX <-> BGRX or more exactly between byte 0 and 2
// of each pixel. `pixmap` must be 4 bytes per pixel.
GPU_GLES2_EXPORT void SwizzleRedAndBlue(const SkPixmap& pixmap);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_REPACK_UTILS_H_
