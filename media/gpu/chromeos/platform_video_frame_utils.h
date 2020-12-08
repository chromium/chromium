// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_UTILS_H_
#define MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_UTILS_H_

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"

namespace gfx {
struct GpuMemoryBufferHandle;
}  // namespace gfx

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace media {

// Create GpuMemoryBuffer-based media::VideoFrame with |buffer_usage|.
// See //media/base/video_frame.h for other parameters.
// |gpu_memory_buffer_factory| must outlive the returned VideoFrame.
MEDIA_GPU_EXPORT scoped_refptr<VideoFrame> CreateGpuMemoryBufferVideoFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage);

// Create platform dependent media::VideoFrame with |buffer_usage|.
// See //media/base/video_frame.h for other parameters.
// |gpu_memory_buffer_factory| must outlive the returned VideoFrame.
MEDIA_GPU_EXPORT scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage);

// Get VideoFrameLayout of platform dependent video frame with |pixel_format|,
// |coded_size| and |buffer_usage|. This function is not cost-free as this
// allocates a platform dependent video frame.
MEDIA_GPU_EXPORT base::Optional<VideoFrameLayout> GetPlatformVideoFrameLayout(
    gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    gfx::BufferUsage buffer_usage);

// Create a shared GPU memory handle to the |video_frame|'s data.
MEDIA_GPU_EXPORT gfx::GpuMemoryBufferHandle CreateGpuMemoryBufferHandle(
    const VideoFrame* video_frame);

// Create a NativePixmap that references the DMA Bufs of |video_frame|. The
// returned pixmap is only a DMA Buf container and should not be used for
// compositing/scanout.
MEDIA_GPU_EXPORT scoped_refptr<gfx::NativePixmapDmaBuf>
CreateNativePixmapDmaBuf(const VideoFrame* video_frame);

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_UTILS_H_
