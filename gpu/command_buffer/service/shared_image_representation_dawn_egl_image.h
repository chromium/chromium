// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_DAWN_EGL_IMAGE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_DAWN_EGL_IMAGE_H_

#include "gpu/command_buffer/service/shared_image_representation.h"

typedef void* EGLImage;

namespace gpu {

class GPU_GLES2_EXPORT SharedImageRepresentationDawnEGLImage
    : public SharedImageRepresentationDawn {
 public:
  SharedImageRepresentationDawnEGLImage(
      std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
          gl_representation,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      const WGPUTextureDescriptor& texture_descriptor);
  ~SharedImageRepresentationDawnEGLImage() override;

 private:
  WGPUTexture BeginAccess(WGPUTextureUsage usage) override;
  void EndAccess() override;

 private:
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
      gl_representation_;
  WGPUDevice device_;
  WGPUTextureDescriptor texture_descriptor_;
  DawnProcTable dawn_procs_;
  WGPUTexture texture_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_DAWN_EGL_IMAGE_H_
