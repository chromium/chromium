// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_DAWN_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_DAWN_REPRESENTATION_H_

#include "gpu/command_buffer/service/shared_image/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {

class ExternalVkImageDawnImageRepresentation : public DawnImageRepresentation {
 public:
  ExternalVkImageDawnImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUTextureFormat dawn_format,
      std::vector<WGPUTextureFormat> view_formats,
      base::ScopedFD memory_fd);

  ExternalVkImageDawnImageRepresentation(
      const ExternalVkImageDawnImageRepresentation&) = delete;
  ExternalVkImageDawnImageRepresentation& operator=(
      const ExternalVkImageDawnImageRepresentation&) = delete;

  ~ExternalVkImageDawnImageRepresentation() override;

  WGPUTexture BeginAccess(WGPUTextureUsage usage) override;
  void EndAccess() override;

 private:
  ExternalVkImageBacking* backing_impl() const {
    return static_cast<ExternalVkImageBacking*>(backing());
  }

  const WGPUDevice device_;
  const WGPUTextureFormat wgpu_format_;
  std::vector<WGPUTextureFormat> view_formats_;
  base::ScopedFD memory_fd_;

  WGPUTexture texture_ = nullptr;

  // TODO(cwallez@chromium.org): Load procs only once when the factory is
  // created and pass a pointer to them around?
  const DawnProcTable dawn_procs_;

  std::vector<ExternalSemaphore> begin_access_semaphores_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_DAWN_REPRESENTATION_H_
