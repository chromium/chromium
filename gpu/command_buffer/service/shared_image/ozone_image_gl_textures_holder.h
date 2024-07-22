// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_GL_TEXTURES_HOLDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_GL_TEXTURES_HOLDER_H_

#include <memory>
#include <vector>

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
class TexturePassthrough;
}  // namespace gles2

// Stores gles2::TexturePassthroughs for OzoneImageBacking.
class GPU_GLES2_EXPORT OzoneImageGLTexturesHolder
    : public base::RefCounted<OzoneImageGLTexturesHolder> {
 public:
  // Creates an OzoneImageGLTexturesHolder with gles2::TexturePassthroughs.
  static scoped_refptr<OzoneImageGLTexturesHolder> CreateAndInitTexturesHolder(
      SharedImageBacking* backing,
      scoped_refptr<gfx::NativePixmap> pixmap);

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

  const scoped_refptr<gles2::TexturePassthrough>& texture(int plane_index) {
    return textures_[plane_index];
  }

  size_t GetNumberOfTextures() const;

 private:
  friend class base::RefCounted<OzoneImageGLTexturesHolder>;

  explicit OzoneImageGLTexturesHolder();
  ~OzoneImageGLTexturesHolder();

  // Initializes this holder with gles2::TexturePassthroughs. On failure,
  // returns false.
  bool Initialize(SharedImageBacking* backing,
                  scoped_refptr<gfx::NativePixmap> pixmap);

  // Creates and stores a gles2::TexturePassthrough. On failure, returns false.
  bool CreateAndStoreTexture(SharedImageBacking* backing,
                             scoped_refptr<gfx::NativePixmap> pixmap,
                             gfx::BufferFormat buffer_format,
                             gfx::BufferPlane buffer_plane,
                             const gfx::Size& size);

  // A counter that is used by OzoneImageBacking to identify how many times this
  // holder has been cached.
  base::CheckedNumeric<size_t> cache_count_ = 0;

  bool context_lost_ = false;
  std::vector<std::unique_ptr<ui::NativePixmapGLBinding>> bindings_;
  std::vector<scoped_refptr<gles2::TexturePassthrough>> textures_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_OZONE_IMAGE_GL_TEXTURES_HOLDER_H_
