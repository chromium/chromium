// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_image_native_pixmap.h"

#include "ui/gfx/color_space.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {

scoped_refptr<GLImageNativePixmap> GLImageNativePixmap::Create(
    const gfx::Size& size,
    gfx::BufferFormat format,
    scoped_refptr<gfx::NativePixmap> pixmap,
    GLenum target,
    GLuint texture_id) {
  DCHECK_GT(texture_id, 0u);

  auto image = base::WrapRefCounted(new GLImageNativePixmap(size));

  if (!image->InitializeFromNativePixmap(format, std::move(pixmap), target,
                                         texture_id)) {
    return nullptr;
  }
  return image;
}

GLImageNativePixmap::GLImageNativePixmap(const gfx::Size& size) : size_(size) {}

GLImageNativePixmap::~GLImageNativePixmap() = default;

bool GLImageNativePixmap::InitializeFromNativePixmap(
    gfx::BufferFormat format,
    scoped_refptr<gfx::NativePixmap> pixmap,
    GLenum target,
    GLuint texture_id) {
  pixmap_gl_binding_ =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->GetCurrentGLOzone()
          ->ImportNativePixmap(std::move(pixmap), format,
                               gfx::BufferPlane::DEFAULT, size_,
                               gfx::ColorSpace(), target, texture_id);

  return !!pixmap_gl_binding_;
}

}  // namespace gpu
