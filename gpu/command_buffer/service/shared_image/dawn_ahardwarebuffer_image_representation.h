// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_AHARDWAREBUFFER_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_AHARDWAREBUFFER_IMAGE_REPRESENTATION_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/android_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {

class DawnAHardwareBufferImageRepresentation : public DawnImageRepresentation {
 public:
  DawnAHardwareBufferImageRepresentation(
      SharedImageManager* manager,
      AndroidImageBacking* backing,
      MemoryTypeTracker* tracker,
      wgpu::Device device,
      wgpu::TextureFormat format,
      std::vector<wgpu::TextureFormat> view_formats,
      AHardwareBuffer* buffer);
  ~DawnAHardwareBufferImageRepresentation() override;

  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) override;
  void EndAccess() override;

 private:
  AndroidImageBacking* android_backing() {
    return static_cast<AndroidImageBacking*>(backing());
  }

  base::android::ScopedHardwareBufferHandle handle_;
  wgpu::Texture texture_;
  wgpu::Device device_;
  wgpu::TextureFormat format_;
  std::vector<wgpu::TextureFormat> view_formats_;
  // There is a SharedTextureMemory per representation with how this works
  // currently. Switching to a single cached SharedTextureMemory for the backing
  // needs some care as multiple representations would use the same VkImage and
  // layout/queue transitions might be problematic.
  wgpu::SharedTextureMemory shared_texture_memory_;
  AccessMode access_mode_ = AccessMode::kNone;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_AHARDWAREBUFFER_IMAGE_REPRESENTATION_H_
