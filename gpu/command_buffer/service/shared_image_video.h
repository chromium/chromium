// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing_android.h"
#include "gpu/command_buffer/service/stream_texture_shared_image_interface.h"
#include "gpu/gpu_gles2_export.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gpu {
class SharedImageRepresentationGLTexture;
class SharedImageRepresentationSkia;
struct Mailbox;

namespace gles2 {
class AbstractTexture;
}  // namespace gles2

// Implementation of SharedImageBacking that renders MediaCodec buffers to a
// TextureOwner or overlay as needed in order to draw them.
class GPU_GLES2_EXPORT SharedImageVideo
    : public SharedImageBackingAndroid,
      public SharedContextState::ContextLostObserver,
      public RefCountedLockHelperDrDc {
 public:
  SharedImageVideo(
      const Mailbox& mailbox,
      const gfx::Size& size,
      const gfx::ColorSpace color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
      scoped_refptr<SharedContextState> shared_context_state,
      bool is_thread_safe,
      scoped_refptr<RefCountedLock> drdc_lock);

  ~SharedImageVideo() override;

  // SharedImageBacking implementation.
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  size_t EstimatedSizeForMemTracking() const override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

  // Returns ycbcr information. This is only valid in vulkan context and
  // nullopt for other context.
  static absl::optional<VulkanYCbCrInfo> GetYcbcrInfo(
      TextureOwner* texture_owner,
      scoped_refptr<SharedContextState> context_state);

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

  std::unique_ptr<gpu::SharedImageRepresentationOverlay> ProduceOverlay(
      gpu::SharedImageManager* manager,
      gpu::MemoryTypeTracker* tracker) override;

  // TODO(vikassoni): Add overlay and AHardwareBuffer representations in future
  // patch. Overlays are anyways using legacy mailbox for now.

 private:
  friend class SharedImageRepresentationGLTextureVideo;
  friend class SharedImageRepresentationGLTexturePassthroughVideo;
  friend class SharedImageRepresentationVideoSkiaGL;
  friend class SharedImageRepresentationVideoSkiaVk;
  friend class SharedImageRepresentationOverlayVideo;

  // Helper method to generate an abstract texture.
  std::unique_ptr<gles2::AbstractTexture> GenAbstractTexture(
      scoped_refptr<SharedContextState> context_state,
      const bool passthrough);

  void BeginGLReadAccess(const GLuint service_id);

  // Creating representations on SharedImageVideo is already thread safe
  // but SharedImageVideo backing can be destroyed on any thread when a
  // backing is used by multiple threads like dr-dc. Hence backing is not
  // guaranteed to be destroyed on the same thread on which it was created.
  // This method ensures that all the member variables of this class are
  // destroyed on the thread in which it was created.
  static void CleanupOnCorrectThread(
      scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii,
      scoped_refptr<SharedContextState> context_state,
      SharedImageVideo* backing,
      base::WaitableEvent* event,
      scoped_refptr<RefCountedLock> lock);

  scoped_refptr<StreamTextureSharedImageInterface> stream_texture_sii_;
  scoped_refptr<SharedContextState> context_state_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageVideo);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_VIDEO_H_
