// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_DAWN_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_DAWN_REPRESENTATION_H_

#include "gpu/command_buffer/service/external_vk_image_backing.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

class ExternalVkImageDawnRepresentation : public SharedImageRepresentationDawn {
 public:
  ExternalVkImageDawnRepresentation(SharedImageManager* manager,
                                    SharedImageBacking* backing,
                                    MemoryTypeTracker* tracker,
                                    WGPUDevice device,
                                    WGPUTextureFormat dawn_format,
                                    int memory_fd,
                                    VkDeviceSize allocation_size,
                                    uint32_t memory_type_index);
  ~ExternalVkImageDawnRepresentation() override;

  WGPUTexture BeginAccess(WGPUTextureUsage usage) override;
  void EndAccess() override;

 private:
  const WGPUDevice device_;
  const WGPUTextureFormat wgpu_format_;
  const int memory_fd_;
  const VkDeviceSize allocation_size_;
  const uint32_t memory_type_index_;

  WGPUTexture texture_ = nullptr;

  // TODO(cwallez@chromium.org): Load procs only once when the factory is
  // created and pass a pointer to them around?
  const DawnProcTable dawn_procs_;

  ExternalVkImageBacking* backing_impl() {
    return static_cast<ExternalVkImageBacking*>(backing());
  }

  DISALLOW_COPY_AND_ASSIGN(ExternalVkImageDawnRepresentation);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_EXTERNAL_VK_IMAGE_DAWN_REPRESENTATION_H_
