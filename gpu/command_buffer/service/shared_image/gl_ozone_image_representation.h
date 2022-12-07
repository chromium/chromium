// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_OZONE_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_OZONE_IMAGE_REPRESENTATION_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
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
    raw_ptr<gles2::Texture, DanglingUntriaged> texture_ = nullptr;
    scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
  };

  static bool BeginAccess(GLenum mode,
                          OzoneImageBacking* ozone_backing,
                          bool& need_end_fence);
  static void EndAccess(bool need_end_fence,
                        GLenum mode,
                        OzoneImageBacking* ozone_backing);

  // Create a NativePixmapGLBinding for the given `pixmap`. On failure, returns
  // nullptr.
  static std::unique_ptr<ui::NativePixmapGLBinding> GetBinding(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat buffer_format,
      gfx::BufferPlane buffer_plane,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GLuint& gl_texture_service_id,
      GLenum& target);

  // Creates a TextureHolder containing the gles2::Texture. On failure, returns
  // nullptr.
  static scoped_refptr<TextureHolder> CreateTextureHolder(
      SharedImageBacking* backing,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat buffer_format,
      gfx::BufferPlane buffer_plane,
      const gfx::Size& size);

  // Creates a TextureHolder containing the gles2::TexturePassthrough. On
  // failure, returns nullptr.
  static scoped_refptr<TextureHolder> CreateTextureHolderPassthrough(
      SharedImageBacking* backing,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat buffer_format,
      gfx::BufferPlane buffer_plane,
      const gfx::Size& size);

  // Creates a vector of TextureHolders for the Texture/TexturePassthrough
  // Representation.
  static std::vector<scoped_refptr<TextureHolder>> CreateShared(
      SharedImageBacking* backing,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferPlane plane,
      bool is_passthrough,
      std::vector<scoped_refptr<TextureHolder>>* cached_texture_holders);
};

// Representation of an Ozone-backed SharedImage that can be accessed as a GL
// texture.
class GLTextureOzoneImageRepresentation : public GLTextureImageRepresentation {
 public:
  using TextureHolder = GLOzoneImageRepresentationShared::TextureHolder;

  // Creates and initializes a GLTextureOzoneImageRepresentation. On failure,
  // returns nullptr.
  static std::unique_ptr<GLTextureOzoneImageRepresentation> Create(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferPlane plane,
      std::vector<scoped_refptr<TextureHolder>>* cached_texture_holders);

  ~GLTextureOzoneImageRepresentation() override;

  // GLTextureImageRepresentation implementation.
  gles2::Texture* GetTexture(int plane_index) override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  GLTextureOzoneImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<scoped_refptr<TextureHolder>> texture_holders);

  OzoneImageBacking* GetOzoneBacking();

  std::vector<scoped_refptr<TextureHolder>> texture_holders_;
  GLenum current_access_mode_ = 0;
  bool need_end_fence_;
};

// Representation of an Ozone-backed SharedImage that can be accessed as a
// GL texture with passthrough.
class GLTexturePassthroughOzoneImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  using TextureHolder = GLOzoneImageRepresentationShared::TextureHolder;

  // Creates and initializes a GLTexturePassthroughOzoneImageRepresentation. On
  // failure, returns nullptr.
  static std::unique_ptr<GLTexturePassthroughOzoneImageRepresentation> Create(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferPlane plane,
      std::vector<scoped_refptr<TextureHolder>>* cached_texture_holders);

  ~GLTexturePassthroughOzoneImageRepresentation() override;

  // GLTexturePassthroughImageRepresentation implementation.
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

 private:
  GLTexturePassthroughOzoneImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      std::vector<scoped_refptr<TextureHolder>> texture_holders);

  OzoneImageBacking* GetOzoneBacking();

  std::vector<scoped_refptr<TextureHolder>> texture_holders_;
  GLenum current_access_mode_ = 0;
  bool need_end_fence_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_OZONE_IMAGE_REPRESENTATION_H_
