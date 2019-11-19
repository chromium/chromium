// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_BUFFER_VALIDATION_H_
#define MEDIA_GPU_BUFFER_VALIDATION_H_

#include "media/base/video_types.h"
#include "media/gpu/media_gpu_export.h"

namespace gfx {
class Size;
struct GpuMemoryBufferHandle;
}  // namespace gfx

namespace media {
// Gets the file size of |fd| and writes in |size|. Returns false on failure.
MEDIA_GPU_EXPORT bool GetFileSize(const int fd, size_t* size);

// Verifies if GpuMemoryBufferHandle is valid.
MEDIA_GPU_EXPORT bool VerifyGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::GpuMemoryBufferHandle& gmb_handle);

}  // namespace media
#endif  // MEDIA_GPU_BUFFER_VALIDATION_H_
