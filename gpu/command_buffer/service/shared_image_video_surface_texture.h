// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_SURFACE_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_SURFACE_TEXTURE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing_android.h"
#include "gpu/command_buffer/service/shared_image_video.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "gpu/gpu_gles2_export.h"

namespace gpu {
class SharedImageRepresentationGLTexture;
class SharedImageRepresentationSkia;
struct Mailbox;

// Implementation of SharedImageBacking that renders MediaCodec buffers to a
// TextureOwner or overlay as needed in order to draw them.
class GPU_GLES2_EXPORT SharedImageVideoSurfaceTexture
    : public SharedImageVideo,
      public SharedContextState::ContextLostObserver {
 public:
  SharedImageVideoSurfaceTexture(
      const Mailbox& mailbox,
      const gfx::Size& size,
      const gfx::ColorSpace color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
      scoped_refptr<SharedContextState> shared_context_state);

  ~SharedImageVideoSurfaceTexture() override;

  // Disallow copy and assign.
  SharedImageVideoSurfaceTexture(const SharedImageVideoSurfaceTexture&) =
      delete;
  SharedImageVideoSurfaceTexture& operator=(
      const SharedImageVideoSurfaceTexture&) = delete;

  // SharedImageBacking implementation.
  size_t EstimatedSizeForMemTracking() const override;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

 protected:
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<gpu::SharedImageRepresentationLegacyOverlay>
  ProduceLegacyOverlay(gpu::SharedImageManager* manager,
                       gpu::MemoryTypeTracker* tracker) override;

 private:
  class SharedImageRepresentationGLTextureVideo;
  class SharedImageRepresentationGLTexturePassthroughVideo;
  class SharedImageRepresentationOverlayVideo;

  void BeginGLReadAccess(const GLuint service_id);

  scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii_;
  scoped_refptr<SharedContextState> context_state_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_main_task_runner_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_SURFACE_TEXTURE_H_
