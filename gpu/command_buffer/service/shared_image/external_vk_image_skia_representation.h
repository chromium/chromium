// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_SKIA_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_SKIA_REPRESENTATION_H_

#include <vector>

#include "gpu/command_buffer/service/external_semaphore.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {

class ExternalVkImageSkiaImageRepresentation
    : public SkiaGaneshImageRepresentation {
 public:
  ExternalVkImageSkiaImageRepresentation(GrDirectContext* gr_context,
                                         SharedImageManager* manager,
                                         SharedImageBacking* backing,
                                         MemoryTypeTracker* tracker);
  ~ExternalVkImageSkiaImageRepresentation() override;

  // SkiaImageRepresentation implementation.
  std::vector<sk_sp<SkSurface>> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      const gfx::Rect& update_rect,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndWriteAccess() override;
  std::vector<sk_sp<GrPromiseImageTexture>> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<skgpu::MutableTextureState>* end_state) override;
  void EndReadAccess() override;

 private:
  ExternalVkImageBacking* backing_impl() const {
    return static_cast<ExternalVkImageBacking*>(backing());
  }
  VulkanFenceHelper* fence_helper() const {
    return backing_impl()->fence_helper();
  }

  std::vector<sk_sp<GrPromiseImageTexture>> BeginAccess(
      bool readonly,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  void EndAccess(bool readonly);

  const scoped_refptr<SharedContextState> context_state_;
  AccessMode access_mode_ = AccessMode::kNone;
  int surface_msaa_count_ = 0;
  std::vector<ExternalSemaphore> begin_access_semaphores_;
  ExternalSemaphore end_access_semaphore_;
  std::vector<sk_sp<SkSurface>> write_surfaces_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_SKIA_REPRESENTATION_H_
