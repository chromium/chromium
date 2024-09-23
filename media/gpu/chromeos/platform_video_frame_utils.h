// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_UTILS_H_
#define MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_UTILS_H_

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "media/base/video_frame.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"

namespace media {

// Returns a GpuMemoryBufferId that's guaranteed to be different from those
// returned by previous calls. This function is thread safe.
MEDIA_GPU_EXPORT gfx::GpuMemoryBufferId GetNextGpuMemoryBufferId();

// Creates a GpuMemoryBufferHandle. This function is thread safe.
gfx::GpuMemoryBufferHandle AllocateGpuMemoryBufferHandle(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    gfx::BufferUsage buffer_usage);

// Creates a STORAGE_GPU_MEMORY_BUFFER VideoFrame backed by a NATIVE_PIXMAP
// GpuMemoryBuffer allocated with |buffer_usage|. See //media/base/video_frame.h
// for the other parameters. This function is thread-safe.
MEDIA_GPU_EXPORT scoped_refptr<VideoFrame> CreateGpuMemoryBufferVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage);

// Creates a STORAGE_GPU_MEMORY_BUFFER VideoFrame from a GpuMemoryBufferHandle.
// See //media/base/video_frame.h for the other parameters. This function is
// thread-safe.
scoped_refptr<VideoFrame> CreateVideoFrameFromGpuMemoryBufferHandle(
    gfx::GpuMemoryBufferHandle gmb_handle,
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage);

// Creates a STORAGE_DMABUFS VideoFrame whose buffer is allocated with
// |buffer_usage|. See //media/base/video_frame.h for the other parameters. This
// function is thread-safe.
MEDIA_GPU_EXPORT scoped_refptr<VideoFrame> CreatePlatformVideoFrame(
    VideoPixelFormat pixel_format,
    const gfx::Size& coded_size,
    const gfx::Rect& visible_rect,
    const gfx::Size& natural_size,
    base::TimeDelta timestamp,
    gfx::BufferUsage buffer_usage);

// Returns the VideoFrameLayout of a VideoFrame allocated with
// CreatePlatformVideoFrame(), i.e., all parameters are forwarded to that
// function (|visible_rect| is set to gfx::Rect(|coded_size|), |natural_size| is
// set to |coded_size|, and |timestamp| is set to base::TimeDelta()). This
// function is not cheap as it allocates a buffer. Returns std::nullopt if the
// buffer allocation fails. This function is thread-safe.
MEDIA_GPU_EXPORT std::optional<VideoFrameLayout> GetPlatformVideoFrameLayout(
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

// Returns either the GPU MemoryBuffer ID or the FD of the first file
// descriptor depending on the storage type.
MEDIA_GPU_EXPORT gfx::GenericSharedMemoryId GetSharedMemoryId(
    const VideoFrame& frame);

// Returns true if |gmb_handle| can be imported into minigbm and false
// otherwise.
bool CanImportGpuMemoryBufferHandle(
    const gfx::Size& size,
    gfx::BufferFormat format,
    const gfx::GpuMemoryBufferHandle& gmb_handle);

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_PLATFORM_VIDEO_FRAME_UTILS_H_
