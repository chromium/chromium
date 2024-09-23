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
      wgpu::Device device,
      wgpu::TextureFormat dawn_format,
      std::vector<wgpu::TextureFormat> view_formats,
      base::ScopedFD memory_fd);

  ExternalVkImageDawnImageRepresentation(
      const ExternalVkImageDawnImageRepresentation&) = delete;
  ExternalVkImageDawnImageRepresentation& operator=(
      const ExternalVkImageDawnImageRepresentation&) = delete;

  ~ExternalVkImageDawnImageRepresentation() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) override;
  void EndAccess() override;

 private:
  ExternalVkImageBacking* backing_impl() const {
    return static_cast<ExternalVkImageBacking*>(backing());
  }

  const wgpu::Device device_;
  const wgpu::TextureFormat wgpu_format_;
  std::vector<wgpu::TextureFormat> view_formats_;
  base::ScopedFD memory_fd_;
  wgpu::Texture texture_;
  std::vector<ExternalSemaphore> begin_access_semaphores_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_EXTERNAL_VK_IMAGE_DAWN_REPRESENTATION_H_
