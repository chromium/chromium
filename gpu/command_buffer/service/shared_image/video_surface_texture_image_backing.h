// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_SURFACE_TEXTURE_IMAGE_BACKING_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_SURFACE_TEXTURE_IMAGE_BACKING_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/android_image_backing.h"
#include "gpu/command_buffer/service/shared_image/android_video_image_backing.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
struct Mailbox;

// Implementation of SharedImageBacking that renders MediaCodec buffers to a
// TextureOwner or overlay as needed in order to draw them.
class GPU_GLES2_EXPORT VideoSurfaceTextureImageBacking
    : public AndroidVideoImageBacking,
      public SharedContextState::ContextLostObserver {
 public:
  VideoSurfaceTextureImageBacking(
      const Mailbox& mailbox,
      const gfx::Size& size,
      const gfx::ColorSpace color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      std::string debug_label,
      scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
      scoped_refptr<SharedContextState> shared_context_state);

  ~VideoSurfaceTextureImageBacking() override;

  // Disallow copy and assign.
  VideoSurfaceTextureImageBacking(const VideoSurfaceTextureImageBacking&) =
      delete;
  VideoSurfaceTextureImageBacking& operator=(
      const VideoSurfaceTextureImageBacking&) = delete;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

 protected:
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<gpu::LegacyOverlayImageRepresentation> ProduceLegacyOverlay(
      gpu::SharedImageManager* manager,
      gpu::MemoryTypeTracker* tracker) override;

 private:
  class GLTextureVideoImageRepresentation;
  class GLTexturePassthroughVideoImageRepresentation;
  class OverlayVideoImageRepresentation;

  void BeginGLReadAccess(const GLuint service_id);

  scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii_;
  scoped_refptr<SharedContextState> context_state_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_main_task_runner_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_SURFACE_TEXTURE_IMAGE_BACKING_H_
