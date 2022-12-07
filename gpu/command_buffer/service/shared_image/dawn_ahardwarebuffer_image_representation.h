// Copyright 2022 The Chromium Authors. All rights reserved.
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
      WGPUDevice device,
      WGPUTextureFormat format,
      std::vector<WGPUTextureFormat> view_formats,
      AHardwareBuffer* buffer,
      scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs);
  ~DawnAHardwareBufferImageRepresentation() override;

  WGPUTexture BeginAccess(WGPUTextureUsage usage) override;
  void EndAccess() override;

 private:
  AndroidImageBacking* android_backing() {
    return static_cast<AndroidImageBacking*>(backing());
  }

  base::android::ScopedHardwareBufferHandle handle_;
  WGPUTexture texture_ = nullptr;
  WGPUDevice device_;
  WGPUTextureFormat format_;
  std::vector<WGPUTextureFormat> view_formats_;
  scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_EGL_IMAGE_REPRESENTATION_H_
