// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_OVERLAY_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_OVERLAY_REPRESENTATION_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/shared_image/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {

class ExternalVkImageOverlayImageRepresentation
    : public OverlayImageRepresentation {
 public:
  ExternalVkImageOverlayImageRepresentation(gpu::SharedImageManager* manager,
                                            ExternalVkImageBacking* backing,
                                            MemoryTypeTracker* tracker);
  ~ExternalVkImageOverlayImageRepresentation() override;
  ExternalVkImageOverlayImageRepresentation(
      const ExternalVkImageOverlayImageRepresentation&) = delete;
  ExternalVkImageOverlayImageRepresentation& operator=(
      const ExternalVkImageOverlayImageRepresentation&) = delete;

 protected:
  // OverlayImageRepresentation implementation
  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override;
  void EndReadAccess(gfx::GpuFenceHandle release_fence) override;

 private:
  void GetAcquireFence(gfx::GpuFenceHandle& fence);

  const raw_ptr<ExternalVkImageBacking> vk_image_backing_;
  std::vector<ExternalSemaphore> read_begin_semaphores_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_OVERLAY_REPRESENTATION_H_
