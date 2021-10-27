// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_OVERLAY_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_OVERLAY_REPRESENTATION_H_

#include "build/build_config.h"
#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

class ExternalVkImageOverlayRepresentation
    : public SharedImageRepresentationOverlay {
 public:
  ExternalVkImageOverlayRepresentation(gpu::SharedImageManager* manager,
                                       ExternalVkImageBacking* backing,
                                       gpu::MemoryTypeTracker* tracker);
  ~ExternalVkImageOverlayRepresentation() override;
  ExternalVkImageOverlayRepresentation(
      const ExternalVkImageOverlayRepresentation&) = delete;
  ExternalVkImageOverlayRepresentation& operator=(
      const ExternalVkImageOverlayRepresentation&) = delete;

 protected:
  // SharedImageRepresentationOverlay implementation
  bool BeginReadAccess(std::vector<gfx::GpuFence>* acquire_fences) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;
  gl::GLImage* GetGLImage() override;

#if defined(OS_ANDROID)
  void NotifyOverlayPromotion(bool promotion, const gfx::Rect& bounds) override;
#endif

 private:
  void GetAcquireFences(std::vector<gfx::GpuFence>* fences);

  ExternalVkImageBacking* const vk_image_backing_;
  std::vector<ExternalSemaphore> read_begin_semaphores_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_OVERLAY_REPRESENTATION_H_
