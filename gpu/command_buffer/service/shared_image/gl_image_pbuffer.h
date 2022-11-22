// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_PBUFFER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_PBUFFER_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/gl_image.h"

namespace gpu {

// GLImagePbuffer is just used to hold references to the underlying
// image content so it can be destroyed when the textures are.
class GPU_GLES2_EXPORT GLImagePbuffer final : public gl::GLImage {
 public:
  GLImagePbuffer(const gfx::Size& size, EGLSurface surface);

  // Safe downcast. Returns nullptr on failure.
  static GLImagePbuffer* FromGLImage(GLImage* image);

  // gl::GLImage implementation.
  gfx::Size GetSize() override;
  unsigned GetInternalFormat() override;
  unsigned GetDataType() override;
  gl::GLImage::Type GetType() const override;
  BindOrCopy ShouldBindOrCopy() override;
  bool BindTexImage(unsigned target) override;
  void ReleaseTexImage(unsigned target) override {}
  bool CopyTexImage(unsigned target) override;
  bool CopyTexSubImage(unsigned target,
                       const gfx::Point& offset,
                       const gfx::Rect& rect) override;
  void SetColorSpace(const gfx::ColorSpace& color_space) override {}
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t process_tracing_id,
                    const std::string& dump_name) override {}

 private:
  ~GLImagePbuffer() override;

  gfx::Size size_;
  EGLSurface surface_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_IMAGE_PBUFFER_H_
