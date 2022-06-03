// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_TEXTURE_PASSTHROUGH_ANDROID_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_TEXTURE_PASSTHROUGH_ANDROID_H_

#include "gpu/command_buffer/service/shared_image_backing_android.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {
class SharedImageBackingAndroid;

class SharedImageRepresentationGLTexturePassthroughAndroid
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughAndroid(
      SharedImageManager* manager,
      SharedImageBackingAndroid* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture);
  ~SharedImageRepresentationGLTexturePassthroughAndroid() override;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;

  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  SharedImageBackingAndroid* android_backing() {
    return static_cast<SharedImageBackingAndroid*>(backing());
  }

  scoped_refptr<gles2::TexturePassthrough> texture_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;

  SharedImageRepresentationGLTexturePassthroughAndroid(
      const SharedImageRepresentationGLTexturePassthroughAndroid&) = delete;
  SharedImageRepresentationGLTexturePassthroughAndroid& operator=(
      const SharedImageRepresentationGLTexturePassthroughAndroid&) = delete;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_TEXTURE_PASSTHROUGH_ANDROID_H_
