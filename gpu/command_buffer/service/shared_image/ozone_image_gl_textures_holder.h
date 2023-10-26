// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_GL_TEXTURES_HOLDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_GL_TEXTURES_HOLDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {
class NativePixmap;
class Size;
}  // namespace gfx

namespace gl {
class GLContext;
}  // namespace gl

namespace ui {
class NativePixmapGLBinding;
}  // namespace ui

namespace gpu {
class SharedImageBacking;

namespace gles2 {
class Texture;
class TexturePassthrough;
}  // namespace gles2

// Stores gles2::Texture(Passthrough)s for OzoneImageBacking. If the
// gl::GLContext passed to the holder has an offscreen surface, the holder will
// use that context to destroy textures it holds. Otherwise, they are destroyed
// on whatever context is current.
class GPU_GLES2_EXPORT OzoneImageGLTexturesHolder
    : public base::RefCounted<OzoneImageGLTexturesHolder> {
 public:
  // Creates an OzoneImageGLTexturesHolder with gles2::Textures or a
  // gles2::TexturePassthroughs.
  static scoped_refptr<OzoneImageGLTexturesHolder> CreateAndInitTexturesHolder(
      gl::GLContext* current_context,
      SharedImageBacking* backing,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferPlane plane,
      bool is_passthrough);

  void OnContextWillDestroy(gl::GLContext* context);
  void MarkContextLost();
  bool WasContextLost();

  gles2::Texture* texture(int plane_index) { return textures_[plane_index]; }
  const scoped_refptr<gles2::TexturePassthrough>& texture_passthrough(
      int plane_index) {
    return textures_passthrough_[plane_index];
  }

  bool is_passthrough() const { return is_passthrough_; }

  size_t GetNumberOfTextures() const;

  bool has_context() const { return !!context_; }

 private:
  friend class base::RefCounted<OzoneImageGLTexturesHolder>;

  OzoneImageGLTexturesHolder(bool is_passthrough,
                             gl::GLContext* current_context);
  ~OzoneImageGLTexturesHolder();

  // Initializes this holder with gles2::Textures or a
  // gles2::TexturePassthroughs (depends on the |is_passthrough|). On failure,
  // returns false.
  bool Initialize(SharedImageBacking* backing,
                  scoped_refptr<gfx::NativePixmap> pixmap,
                  gfx::BufferPlane plane);

  // Creates and stores a gles2::Texture or a gles2::TexturePassthrough (depends
  // on the |is_passthrough|). On failure, returns false.
  bool CreateAndStoreTexture(SharedImageBacking* backing,
                             scoped_refptr<gfx::NativePixmap> pixmap,
                             gfx::BufferFormat buffer_format,
                             gfx::BufferPlane buffer_plane,
                             const gfx::Size& size);

  void MaybeDestroyTexturesOnContext();

  // The context the textures were created on. Can be null if the texture holder
  // doesn't need to make it current when being destroyed. This has to be a raw
  // ptr as we want the GLContext to be destroyed. If that happens, the texture
  // holder is notified by the OzoneImageBacking about that.
  raw_ptr<gl::GLContext> context_;

  // Helps to identify this holder.
  const bool is_passthrough_;

  bool context_lost_ = false;
  bool context_destroyed_ = false;
  std::vector<std::unique_ptr<ui::NativePixmapGLBinding>> bindings_;
  std::vector<raw_ptr<gles2::Texture>> textures_;
  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures_passthrough_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_GL_TEXTURES_HOLDER_H_
