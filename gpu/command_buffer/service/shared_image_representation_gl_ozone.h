// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_OZONE_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_OZONE_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_ozone.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_image_native_pixmap.h"

namespace gpu {

class SharedImageRepresentationGLOzoneShared {
 public:
  static bool BeginAccess(GLenum mode,
                          SharedImageBackingOzone* ozone_backing,
                          bool& need_end_fence);
  static void EndAccess(bool need_end_fence,
                        GLenum mode,
                        SharedImageBackingOzone* ozone_backing);
  static absl::optional<GLuint> SetupTexture(
      scoped_refptr<gl::GLImageNativePixmap> image,
      GLenum target);
  static scoped_refptr<gl::GLImageNativePixmap> CreateGLImage(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat buffer_format,
      gfx::BufferPlane plane,
      gfx::Size size);
};

// Representation of an Ozone-backed SharedImage that can be accessed as a
// GL texture.
class SharedImageRepresentationGLTextureOzone
    : public SharedImageRepresentationGLTexture {
 public:
  // Creates and initializes a SharedImageRepresentationGLTextureOzone. On
  // failure, returns nullptr.
  static std::unique_ptr<SharedImageRepresentationGLTextureOzone> Create(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gfx::NativePixmap> pixmap,
      viz::ResourceFormat format,
      gfx::BufferPlane plane);

  ~SharedImageRepresentationGLTextureOzone() override;

  // SharedImageRepresentationGLTexture implementation.
  gles2::Texture* GetTexture() override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  SharedImageRepresentationGLTextureOzone(SharedImageManager* manager,
                                          SharedImageBacking* backing,
                                          MemoryTypeTracker* tracker,
                                          gles2::Texture* texture);

  SharedImageBackingOzone* ozone_backing() {
    return static_cast<SharedImageBackingOzone*>(backing());
  }

  gles2::Texture* texture_;
  GLenum current_access_mode_ = 0;
  bool need_end_fence_;
};

// Representation of an Ozone-backed SharedImage that can be accessed as a
// GL texture with passthrough.
class SharedImageRepresentationGLTexturePassthroughOzone
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  // Creates and initializes a
  // SharedImageRepresentationGLTexturePassthroughOzone. On failure, returns
  // nullptr.
  static std::unique_ptr<SharedImageRepresentationGLTexturePassthroughOzone>
  Create(SharedImageManager* manager,
         SharedImageBacking* backing,
         MemoryTypeTracker* tracker,
         scoped_refptr<gfx::NativePixmap> pixmap,
         viz::ResourceFormat format,
         gfx::BufferPlane plane);

  ~SharedImageRepresentationGLTexturePassthroughOzone() override;

  // SharedImageRepresentationGLTexturePassthrough implementation.
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  SharedImageRepresentationGLTexturePassthroughOzone(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough);

  SharedImageBackingOzone* ozone_backing() {
    return static_cast<SharedImageBackingOzone*>(backing());
  }

  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  GLenum current_access_mode_ = 0;
  bool need_end_fence_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_GL_OZONE_H_
