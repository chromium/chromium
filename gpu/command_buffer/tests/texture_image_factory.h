// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_TEXTURE_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_TESTS_TEXTURE_IMAGE_FACTORY_H_

#include "gpu/command_buffer/service/image_factory.h"

namespace gpu {

// The images created by this factory have no inherent storage. When the image
// is bound to a texture, storage is allocated for the texture via glTexImage2D.
class TextureImageFactory : public gpu::ImageFactory {
 public:
  bool SupportsCreateAnonymousImage() const override;
  scoped_refptr<gl::GLImage> CreateAnonymousImage(const gfx::Size& size,
                                                  gfx::BufferFormat format,
                                                  gfx::BufferUsage usage,
                                                  SurfaceHandle surface_handle,
                                                  bool* is_cleared) override;
  unsigned RequiredTextureType() override;
  bool SupportsFormatRGB() override;

  void SetRequiredTextureType(unsigned type);

 private:
  unsigned required_texture_type_ = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_TEXTURE_IMAGE_FACTORY_H_
