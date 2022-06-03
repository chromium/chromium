// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_factory.h"

#include "ui/gl/gl_image.h"

namespace gpu {

ImageFactory::ImageFactory() = default;

ImageFactory::~ImageFactory() = default;

bool ImageFactory::SupportsCreateAnonymousImage() const {
  return false;
}

scoped_refptr<gl::GLImage> ImageFactory::CreateAnonymousImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    SurfaceHandle surface_handle,
    bool* is_cleared) {
  NOTREACHED();
  return nullptr;
}

unsigned ImageFactory::RequiredTextureType() {
  NOTIMPLEMENTED();
  return 0;
}

bool ImageFactory::SupportsFormatRGB() {
  return true;
}

}  // namespace gpu
