// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_RENDERABLE_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
#define MEDIA_VIDEO_RENDERABLE_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/gpu/GrTypes.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {
class ColorSpace;
class GpuMemoryBuffer;
class Size;
}  // namespace gfx

namespace gpu {
class GpuMemoryBufferManager;
class SharedImageInterface;
struct Mailbox;
struct SyncToken;
}  // namespace gpu

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

    // Create a SharedImage representation of a plane of a GpuMemoryBuffer
    // allocated by this interface. Populate `mailbox` and `sync_token`.
    virtual void CreateSharedImage(gfx::GpuMemoryBuffer* gpu_memory_buffer,
                                   gfx::BufferPlane plane,
                                   const gfx::ColorSpace& color_space,
                                   GrSurfaceOrigin surface_origin,
                                   SkAlphaType alpha_type,
                                   uint32_t usage,
                                   gpu::Mailbox& mailbox,
                                   gpu::SyncToken& sync_token) = 0;

    // Destroy a SharedImage created by this interface.
    virtual void DestroySharedImage(const gpu::SyncToken& sync_token,
                                    const gpu::Mailbox& mailbox) = 0;

    virtual ~Context() = default;
  };

  // Create a frame pool. The supplied `context` will live until all frames
  // created by the pool have been destroyed (so it may outlive the returned
  // pool).
  static std::unique_ptr<RenderableGpuMemoryBufferVideoFramePool> Create(
      std::unique_ptr<Context> context);

  // Returns a GpuMemoryBuffer-backed VideoFrame that can be rendered to. This
  // may return nullptr on an unsupported parameter, or may return nullptr
  // forever in response to a context lost.
  virtual scoped_refptr<VideoFrame> MaybeCreateVideoFrame(
      const gfx::Size& coded_size,
      const gfx::ColorSpace& color_space) = 0;

  virtual ~RenderableGpuMemoryBufferVideoFramePool() = default;
};

}  // namespace media

#endif  // MEDIA_VIDEO_RENDERABLE_GPU_MEMORY_BUFFER_VIDEO_FRAME_POOL_H_
