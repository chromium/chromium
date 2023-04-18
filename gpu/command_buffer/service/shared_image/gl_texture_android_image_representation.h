// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_ANDROID_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_ANDROID_IMAGE_REPRESENTATION_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/shared_image/android_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gl/scoped_egl_image.h"

namespace gpu {
class AndroidImageBacking;

// A generic GL Texture representation which can be used by any backing on
// Android.
class GLTextureAndroidImageRepresentation
    : public GLTextureImageRepresentation {
 public:
  GLTextureAndroidImageRepresentation(SharedImageManager* manager,
                                      AndroidImageBacking* backing,
                                      MemoryTypeTracker* tracker,
                                      gl::ScopedEGLImage egl_image,
                                      gles2::Texture* texture);
  ~GLTextureAndroidImageRepresentation() override;

  GLTextureAndroidImageRepresentation(
      const GLTextureAndroidImageRepresentation&) = delete;
  GLTextureAndroidImageRepresentation& operator=(
      const GLTextureAndroidImageRepresentation&) = delete;

  gles2::Texture* GetTexture(int plane_index) override;

  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  AndroidImageBacking* android_backing() {
    return static_cast<AndroidImageBacking*>(backing());
  }

  gl::ScopedEGLImage egl_image_;
  raw_ptr<gles2::Texture> texture_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_ANDROID_IMAGE_REPRESENTATION_H_
