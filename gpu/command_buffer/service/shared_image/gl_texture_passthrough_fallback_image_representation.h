// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_PASSTHROUGH_FALLBACK_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_PASSTHROUGH_FALLBACK_IMAGE_REPRESENTATION_H_

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

#include "gpu/command_buffer/service/shared_image/gl_texture_holder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace gl {
class ProgressReporter;
}  // namespace gl

namespace gpu {

class GLTexturePassthroughFallbackImageRepresentation
    : public GLTexturePassthroughImageRepresentation {
 public:
  GLTexturePassthroughFallbackImageRepresentation(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      gl::ProgressReporter* progress_reporter,
      const GLFormatCaps& gl_format_caps);
  ~GLTexturePassthroughFallbackImageRepresentation() override;

  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough(
      int plane_index) override;

 private:
  bool BeginAccess(GLenum mode) override;
  void EndAccess() override;

  std::vector<GLTextureHolder> plane_textures_;
  std::vector<SkBitmap> plane_bitmaps_;
  std::vector<SkPixmap> plane_pixmaps_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_PASSTHROUGH_FALLBACK_IMAGE_REPRESENTATION_H_
