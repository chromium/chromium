// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_GL_TEXTURE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_GL_TEXTURE_REPRESENTATION_H_

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

namespace gpu {

class GPU_GLES2_EXPORT DawnGLTextureRepresentation
    : public DawnImageRepresentation {
 public:
  DawnGLTextureRepresentation(
      std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      wgpu::Device device);
  ~DawnGLTextureRepresentation() override;

 private:
  wgpu::Texture BeginAccess(wgpu::TextureUsage usage,
                            wgpu::TextureUsage internal_usage) override;
  void EndAccess() override;

 private:
  std::unique_ptr<GLTextureImageRepresentationBase> gl_representation_;
  const wgpu::Device device_;
  wgpu::Texture texture_ = nullptr;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_GL_TEXTURE_REPRESENTATION_H_
