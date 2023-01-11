// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_BUFFER_VALIDATION_H_
#define MEDIA_GPU_BUFFER_VALIDATION_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "media/base/video_types.h"

namespace gfx {
class Size;
struct GpuMemoryBufferHandle;
}  // namespace gfx

namespace media {

using GetFileSizeCBForTesting = base::RepeatingCallback<size_t()>;

// Gets the file size of |fd| and writes in |size|. Returns false on failure.
COMPONENT_EXPORT(MEDIA_GPU_BUFFER_VALIDATION)
bool GetFileSize(const int fd, size_t* size);

// Verifies if GpuMemoryBufferHandle is valid.
COMPONENT_EXPORT(MEDIA_GPU_BUFFER_VALIDATION)
bool VerifyGpuMemoryBufferHandle(
    media::VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::GpuMemoryBufferHandle& gmb_handle,
    GetFileSizeCBForTesting file_size_cb_for_testing =
        GetFileSizeCBForTesting());

}  // namespace media
#endif  // MEDIA_GPU_BUFFER_VALIDATION_H_
