// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_TEXTURE_ANDROID_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_TEXTURE_ANDROID_H_

#include "gpu/command_buffer/service/shared_image_backing_android.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {
class SharedImageBackingAndroid;

// A generic GL Texture representation which can be used by any backing on
// Android.
class SharedImageRepresentationGLTextureAndroid
    : public SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureAndroid(SharedImageManager* manager,
                                            SharedImageBackingAndroid* backing,
                                            MemoryTypeTracker* tracker,
                                            gles2::Texture* texture);
  ~SharedImageRepresentationGLTextureAndroid() override;

  gles2::Texture* GetTexture() override;

  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  SharedImageBackingAndroid* android_backing() {
    return static_cast<SharedImageBackingAndroid*>(backing());
  }

  gles2::Texture* const texture_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;

  SharedImageRepresentationGLTextureAndroid(
      const SharedImageRepresentationGLTextureAndroid&) = delete;
  SharedImageRepresentationGLTextureAndroid& operator=(
      const SharedImageRepresentationGLTextureAndroid&) = delete;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_TEXTURE_ANDROID_H_
