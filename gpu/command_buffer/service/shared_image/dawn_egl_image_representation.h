// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_EGL_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_EGL_IMAGE_REPRESENTATION_H_

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

typedef void* EGLImage;

namespace gpu {

class GPU_GLES2_EXPORT DawnEGLImageRepresentation
    : public DawnImageRepresentation {
 public:
  DawnEGLImageRepresentation(
      std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      WGPUDevice device);
  ~DawnEGLImageRepresentation() override;

 private:
  WGPUTexture BeginAccess(WGPUTextureUsage usage) override;
  void EndAccess() override;

 private:
  std::unique_ptr<GLTextureImageRepresentationBase> gl_representation_;
  WGPUDevice device_;
  DawnProcTable dawn_procs_;
  WGPUTexture texture_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_EGL_IMAGE_REPRESENTATION_H_
