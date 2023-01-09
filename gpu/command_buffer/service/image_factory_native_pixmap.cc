// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/image_factory_native_pixmap.h"

#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_usage_util.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace gpu {

ImageFactoryNativePixmap::ImageFactoryNativePixmap() = default;

ImageFactoryNativePixmap::~ImageFactoryNativePixmap() = default;

bool ImageFactoryNativePixmap::SupportsCreateAnonymousImage() const {
  // Platforms may not support native pixmaps.
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformRuntimeProperties()
      .supports_native_pixmaps;
}

scoped_refptr<gl::GLImage> ImageFactoryNativePixmap::CreateAnonymousImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    SurfaceHandle surface_handle,
    bool* is_cleared) {
  scoped_refptr<gfx::NativePixmap> pixmap;
  pixmap =
      ui::OzonePlatform::GetInstance()
          ->GetSurfaceFactoryOzone()
          ->CreateNativePixmap(surface_handle, nullptr, size, format, usage);
  if (!pixmap.get()) {
    LOG(ERROR) << "Failed to create pixmap " << size.ToString() << ", "
               << gfx::BufferFormatToString(format) << ", usage "
               << gfx::BufferUsageToString(usage);
    return nullptr;
  }
  auto image = gl::GLImageNativePixmap::Create(size, format, std::move(pixmap));
  if (!image) {
    LOG(ERROR) << "Failed to create GLImage " << size.ToString() << ", "
               << gfx::BufferFormatToString(format) << ", usage "
               << gfx::BufferUsageToString(usage);
    return nullptr;
  }
  *is_cleared = true;
  return image;
}

unsigned ImageFactoryNativePixmap::RequiredTextureType() {
  return GL_TEXTURE_2D;
}

ImageFactoryNativePixmap*
ImageFactoryNativePixmap::AsImageFactoryNativePixmap() {
  return this;
}

}  // namespace gpu
