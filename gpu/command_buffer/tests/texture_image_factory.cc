// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/texture_image_factory.h"

namespace gpu {

bool TextureImageFactory::SupportsCreateAnonymousImage() const {
  return false;
}

scoped_refptr<gl::GLImage> TextureImageFactory::CreateAnonymousImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    SurfaceHandle surface_handle,
    bool* is_cleared) {
  NOTREACHED();
  return nullptr;
}

unsigned TextureImageFactory::RequiredTextureType() {
  return required_texture_type_;
}

bool TextureImageFactory::SupportsFormatRGB() {
  return false;
}

void TextureImageFactory::SetRequiredTextureType(unsigned type) {
  required_texture_type_ = type;
}

}  // namespace gpu
