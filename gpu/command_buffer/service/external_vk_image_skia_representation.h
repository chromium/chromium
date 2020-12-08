// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_SKIA_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_SKIA_REPRESENTATION_H_

#include <vector>

#include "gpu/command_buffer/service/external_semaphore.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

class ExternalVkImageSkiaRepresentation : public SharedImageRepresentationSkia {
 public:
  ExternalVkImageSkiaRepresentation(SharedImageManager* manager,
                                    SharedImageBacking* backing,
                                    MemoryTypeTracker* tracker);
  ~ExternalVkImageSkiaRepresentation() override;

  // SharedImageRepresentationSkia implementation.
  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndWriteAccess(sk_sp<SkSurface> surface) override;
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndReadAccess() override;

 private:
  ExternalVkImageBacking* backing_impl() const {
    return static_cast<ExternalVkImageBacking*>(backing());
  }
  VulkanFenceHelper* fence_helper() const {
    return backing_impl()->fence_helper();
  }

  sk_sp<SkPromiseImageTexture> BeginAccess(
      bool readonly,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  void EndAccess(bool readonly);

  enum AccessMode {
    kNone = 0,
    kRead = 1,
    kWrite = 2,
  };
  AccessMode access_mode_ = kNone;
  int surface_msaa_count_ = 0;
  std::vector<ExternalSemaphore> begin_access_semaphores_;
  ExternalSemaphore end_access_semaphore_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_SKIA_REPRESENTATION_H_
