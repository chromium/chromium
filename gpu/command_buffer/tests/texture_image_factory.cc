// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/tests/texture_image_factory.h"

#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"

namespace gpu {

// An image that allocates storage for the texture using glTexImage2D.
class TextureImage : public gl::GLImage {
 public:
  explicit TextureImage(const gfx::Size& size) : size_(size) {}

  gfx::Size GetSize() override { return size_; }
  unsigned GetInternalFormat() override { return GL_RGBA; }
  unsigned GetDataType() override { return GL_UNSIGNED_BYTE; }
  BindOrCopy ShouldBindOrCopy() override { return BIND; }
  bool BindTexImage(unsigned target) override {
    glTexImage2D(target,
                 0,  // mip level
                 GetInternalFormat(), size_.width(), size_.height(),
                 0,  // border
                 GetDataFormat(), GetDataType(), nullptr);
    return true;
  }
  bool BindTexImageWithInternalformat(unsigned target,
                                      unsigned internal_format) override {
    glTexImage2D(target,
                 0,  // mip level
                 GetInternalFormat(), size_.width(), size_.height(),
                 0,  // border
                 GetDataFormat(), GetDataType(), nullptr);
    return true;
  }
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override {
    NOTREACHED();
    return false;
  }
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override {
    return false;
  }
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int z_order,
                            gfx::OverlayTransform transform,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override {
    return false;
  }
  void SetColorSpace(const gfx::ColorSpace& color_space) override {}
  void Flush() override {}
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override {}

 private:
  ~TextureImage() override = default;
  gfx::Size size_;
};

scoped_refptr<gl::GLImage> TextureImageFactory::CreateImageForGpuMemoryBuffer(
    gfx::GpuMemoryBufferHandle handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    int client_id,
    SurfaceHandle surface_handle) {
  return nullptr;
}

bool TextureImageFactory::SupportsCreateAnonymousImage() const {
  return true;
}

scoped_refptr<gl::GLImage> TextureImageFactory::CreateAnonymousImage(
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool* is_cleared) {
  *is_cleared = true;
  return new TextureImage(size);
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
