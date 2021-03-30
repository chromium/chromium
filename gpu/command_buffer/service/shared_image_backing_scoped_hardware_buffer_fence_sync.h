// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_SCOPED_HARDWARE_BUFFER_FENCE_SYNC_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_SCOPED_HARDWARE_BUFFER_FENCE_SYNC_H_

#include <memory>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image_backing_android.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/gpu_fence_handle.h"

namespace gpu {
class SharedImageRepresentationGLTextureScopedHardwareBufferFenceSync;
class
    SharedImageRepresentationGLTexturePassthroughScopedHardwareBufferFenceSync;

// Backing which wraps ScopedHardwareBufferFenceSync object and allows
// producing/using different representations from it. This backing is not thread
// safe.
class GPU_GLES2_EXPORT SharedImageBackingScopedHardwareBufferFenceSync
    : public SharedImageBackingAndroid {
 public:
  SharedImageBackingScopedHardwareBufferFenceSync(
      std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
          scoped_hardware_buffer,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      bool is_thread_safe);
  ~SharedImageBackingScopedHardwareBufferFenceSync() override;

  // SharedImageBacking implementation.
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) override;
  size_t EstimatedSizeForMemTracking() const override;

  AHardwareBuffer* BeginOverlayAccess(gfx::GpuFenceHandle& begin_read_fence);
  void EndOverlayAccess(gfx::GpuFenceHandle release_fence);

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

  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

 private:
  friend class SharedImageRepresentationGLTextureScopedHardwareBufferFenceSync;
  friend class
      SharedImageRepresentationGLTexturePassthroughScopedHardwareBufferFenceSync;
  friend class SharedImageRepresentationSkiaVkScopedHardwareBufferFenceSync;

  std::unique_ptr<
      SharedImageRepresentationGLTextureScopedHardwareBufferFenceSync>
  GenGLTextureRepresentation(SharedImageManager* manager,
                             MemoryTypeTracker* tracker);
  std::unique_ptr<
      SharedImageRepresentationGLTexturePassthroughScopedHardwareBufferFenceSync>
  GenGLTexturePassthroughRepresentation(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker);
  bool BeginGLReadAccess();
  void EndGLReadAccess();
  void EndSkiaReadAccess();

  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;

  // Fence which needs to be waited upon before reading the
  // |scoped_hardware_buffer_|.
  base::ScopedFD ahb_read_fence_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingScopedHardwareBufferFenceSync);
};

}  // namespace gpu
#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_SCOPED_HARDWARE_BUFFER_FENCE_SYNC_H_
