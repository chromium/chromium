// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_GL_TEXTURES_HOLDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_GL_TEXTURES_HOLDER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/checked_math.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gfx/buffer_types.h"

namespace gfx {
class NativePixmap;
class Size;
}  // namespace gfx

namespace ui {
class NativePixmapGLBinding;
}  // namespace ui

namespace gpu {
class SharedImageBacking;

namespace gles2 {
class Texture;
class TexturePassthrough;
}  // namespace gles2

// Stores gles2::Texture(Passthrough)s for OzoneImageBacking.
class GPU_GLES2_EXPORT OzoneImageGLTexturesHolder
    : public base::RefCounted<OzoneImageGLTexturesHolder> {
 public:
  // Creates an OzoneImageGLTexturesHolder with gles2::Textures or a
  // gles2::TexturePassthroughs.
  static scoped_refptr<OzoneImageGLTexturesHolder> CreateAndInitTexturesHolder(
      SharedImageBacking* backing,
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferPlane plane,
      bool is_passthrough);

  void MarkContextLost();
  bool WasContextLost();

  // See |cache_counter_|.
  void OnAddedToCache();
  void OnRemovedFromCache();
  size_t GetCacheCount() const;

  // Destroys textures that this holder holds. The caller must ensure it make a
  // correct context as current. Eg - the context which was used when this
  // holder was created or the compatible context that was used to reuse this
  // holder.
  void DestroyTextures();

  gles2::Texture* texture(int plane_index) { return textures_[plane_index]; }
  const scoped_refptr<gles2::TexturePassthrough>& texture_passthrough(
      int plane_index) {
    return textures_passthrough_[plane_index];
  }

  bool is_passthrough() const { return is_passthrough_; }

  size_t GetNumberOfTextures() const;

 private:
  friend class base::RefCounted<OzoneImageGLTexturesHolder>;

  explicit OzoneImageGLTexturesHolder(bool is_passthrough);
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

  // Helps to identify this holder.
  const bool is_passthrough_;

  // A counter that is used by OzoneImageBacking to identify how many times this
  // holder has been cached.
  base::CheckedNumeric<size_t> cache_count_ = 0;

  bool context_lost_ = false;
  std::vector<std::unique_ptr<ui::NativePixmapGLBinding>> bindings_;
  std::vector<raw_ptr<gles2::Texture>> textures_;
  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures_passthrough_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_GL_TEXTURES_HOLDER_H_
