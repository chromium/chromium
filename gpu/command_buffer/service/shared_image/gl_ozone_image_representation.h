// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_OZONE_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_OZONE_IMAGE_REPRESENTATION_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_image_native_pixmap.h"
#include "ui/ozone/public/native_pixmap_gl_binding.h"

namespace gpu {
class OzoneImageBacking;

class GLOzoneImageRepresentationShared {
 public:
  class TextureHolder : public base::RefCounted<TextureHolder> {
   public:
    TextureHolder(std::unique_ptr<ui::NativePixmapGLBinding> binding,
                  gles2::Texture* texture);
    TextureHolder(std::unique_ptr<ui::NativePixmapGLBinding> binding,
                  scoped_refptr<gles2::TexturePassthrough> texture_passthrough);
    void MarkContextLost();
    bool WasContextLost();

    gles2::Texture* texture() { return texture_; }
    const scoped_refptr<gles2::TexturePassthrough>& texture_passthrough() {
      return texture_passthrough_;
    }

   private:
    friend class base::RefCounted<TextureHolder>;

    ~TextureHolder();

    bool context_lost_ = false;
    std::unique_ptr<ui::NativePixmapGLBinding> binding_;
    raw_ptr<gles2::Texture> texture_ = nullptr;
    scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  };

  static bool BeginAccess(GLenum mode,
                          OzoneImageBacking* ozone_backing,
                          bool& need_end_fence);
  static void EndAccess(bool need_end_fence,
                        GLenum mode,
                        OzoneImageBacking* ozone_backing);
  static std::unique_ptr<ui::NativePixmapGLBinding> GetBinding(
      SharedImageBacking* backing,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferPlane plane,
      GLuint& gl_texture_service_id,
      GLenum& target);
};

// Representation of an Ozone-backed SharedImage that can be accessed as a
// GL texture.
class GLTextureOzoneImageRepresentation : public GLTextureImageRepresentation {
 public:
  using TextureHolder = GLOzoneImageRepresentationShared::TextureHolder;

  // Creates and initializes a GLTextureOzoneImageRepresentation. On
  // failure, returns nullptr.
  static std::unique_ptr<GLTextureOzoneImageRepresentation> Create(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferPlane plane,
      scoped_refptr<TextureHolder>* cached_texture_holder);

  ~GLTextureOzoneImageRepresentation() override;

  // GLTextureImageRepresentation implementation.
  gles2::Texture* GetTexture() override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  GLTextureOzoneImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<TextureHolder> texture_holder);

  OzoneImageBacking* GetOzoneBacking();

  scoped_refptr<TextureHolder> texture_holder_;
  GLenum current_access_mode_ = 0;
  bool need_end_fence_;
};

// Representation of an Ozone-backed SharedImage that can be accessed as a
// GL texture with passthrough.
class GLTexturePassthroughOzoneImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  using TextureHolder = GLOzoneImageRepresentationShared::TextureHolder;

  // Creates and initializes a
  // GLTexturePassthroughOzoneImageRepresentation. On failure, returns
  // nullptr.
  static std::unique_ptr<GLTexturePassthroughOzoneImageRepresentation> Create(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferPlane plane,
      scoped_refptr<TextureHolder>* cached_texture_holder);

  ~GLTexturePassthroughOzoneImageRepresentation() override;

  // GLTexturePassthroughImageRepresentation implementation.
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  GLTexturePassthroughOzoneImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<TextureHolder> texture_holder);

  OzoneImageBacking* GetOzoneBacking();

  scoped_refptr<TextureHolder> texture_holder_;
  GLenum current_access_mode_ = 0;
  bool need_end_fence_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_OZONE_IMAGE_REPRESENTATION_H_
