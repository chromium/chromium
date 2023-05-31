// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_CHROMEOS_COMPRESSED_GPU_MEMORY_BUFFER_VIDEO_FRAME_UTILS_H_
#define MEDIA_GPU_CHROMEOS_CHROMEOS_COMPRESSED_GPU_MEMORY_BUFFER_VIDEO_FRAME_UTILS_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace gfx {
class Rect;
class Size;
class GpuMemoryBuffer;
}  // namespace gfx

namespace media {

class VideoFrame;

// Returns a VideoFrame that wraps a GpuMemoryBuffer that references a
// compressed dma-buf. This is used for Intel media compression in which the
// number of planes for NV12/P010 buffers is 4 instead of 2. If we were to use
// VideoFrame::WrapExternalGpuMemoryBuffer(), we would run into a validation
// error because of the extra planes. Instead of loosening the validation there,
// we introduced WrapChromeOSCompressedGpuMemoryBufferAsVideoFrame() which
// contains this loosened validation. That way, it's easier to reason about the
// safety of the code. A more detailed discussion is available in
// go/brainstorming-for-intel-mmc-on-chrome.
scoped_refptr<VideoFrame> WrapChromeOSCompressedGpuMemoryBufferAsVideoFrame(
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer,
    base::TimeDelta timestamp);
}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_CHROMEOS_COMPRESSED_GPU_MEMORY_BUFFER_VIDEO_FRAME_UTILS_H_
