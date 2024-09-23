// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_RENDERABLE_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
#define MEDIA_VIDEO_RENDERABLE_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {
class ColorSpace;
class GpuMemoryBuffer;
class Size;
}  // namespace gfx

namespace gpu {
class ClientSharedImage;
class GpuMemoryBufferManager;
class SharedImageInterface;
struct SyncToken;
}  // namespace gpu

namespace viz {
class SharedImageFormat;
}

namespace media {

class VideoFrame;

// A video frame pool that returns GpuMemoryBuffer-backed VideoFrames. All
// access to this class must be on the thread on which it was created.
class MEDIA_EXPORT RenderableGpuMemoryBufferVideoFramePool {
 public:
  // Interface to GPU functionality. This particular interface (as opposed to,
  // say, exposing a GpuMemoryBufferManager and SharedImageInterface) is
  // chosen for testing.
  class Context {
   public:
    // Allocate a GpuMemoryBuffer.
    virtual std::unique_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
        const gfx::Size& size,
        gfx::BufferFormat format,
        gfx::BufferUsage usage) = 0;

    // Create a SharedImage representation with format `si_format` of a
    // GpuMemoryBuffer allocated by this interface.
    // Return a ClientSharedImage pointer. Populate `sync_token`.
    virtual scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
        gfx::GpuMemoryBuffer* gpu_memory_buffer,
        const viz::SharedImageFormat& si_format,
        const gfx::ColorSpace& color_space,
        GrSurfaceOrigin surface_origin,
        SkAlphaType alpha_type,
        gpu::SharedImageUsageSet usage,
        gpu::SyncToken& sync_token) = 0;

    // Used to create a Mappable shared image.
    virtual scoped_refptr<gpu::ClientSharedImage> CreateSharedImage(
        const gfx::Size& size,
        gfx::BufferUsage buffer_usage,
        const viz::SharedImageFormat& si_format,
        const gfx::ColorSpace& color_space,
        GrSurfaceOrigin surface_origin,
        SkAlphaType alpha_type,
        gpu::SharedImageUsageSet usage,
        gpu::SyncToken& sync_token) = 0;

    // Destroy a SharedImage created by this interface.
    virtual void DestroySharedImage(
        const gpu::SyncToken& sync_token,
        scoped_refptr<gpu::ClientSharedImage> shared_image,
        const bool is_mappable_si_enabled) = 0;

    virtual ~Context() = default;
  };

  // Create a frame pool. The supplied `context` will live until all frames
  // created by the pool have been destroyed (so it may outlive the returned
  // pool). Only NV12 and ARGB formats are supported.
  static std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool> Create(
      std::unique_ptr<Context> context,
      VideoPixelFormat format = PIXEL_FORMAT_NV12);

  // Returns a GpuMemoryBuffer-backed VideoFrame that can be rendered to. This
  // may return nullptr on an unsupported parameter, or may return nullptr
  // forever in response to a context lost.
  virtual scoped_refptr<VideoFrame> MaybeCreateVideoFrame(
      const gfx::Size& coded_size,
      const gfx::ColorSpace& color_space) = 0;

  // Returns whether MappableSI is enabled for
  // RenderableGpuMemoryBufferVideoFramePool. This method is only used by tests.
  virtual bool IsMappableSIEnabledForTesting() const = 0;

  virtual ~RenderableGpuMemoryBufferVideoFramePool() = default;
};

}  // namespace media

#endif  // MEDIA_VIDEO_RENDERABLE_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
