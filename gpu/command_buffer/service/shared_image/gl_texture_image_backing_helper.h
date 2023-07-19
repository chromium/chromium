// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_HELPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

// Sets GL state for readback.
class ScopedPackState {
 public:
  explicit ScopedPackState(int pack_row_length = 0, int pack_alignment = 4);

  ScopedPackState(const ScopedPackState&) = delete;
  ScopedPackState& operator=(const ScopedPackState&) = delete;

  ~ScopedPackState();

 private:
  const raw_ptr<gl::GLApi> api_;

  GLint pack_buffer_ = 0;
  absl::optional<gl::ScopedPixelStore> pack_alignment_;
  absl::optional<gl::ScopedPixelStore> pack_row_length_;
  absl::optional<gl::ScopedPixelStore> pack_skip_pixels_;
  absl::optional<gl::ScopedPixelStore> pack_skip_rows_;
};

// Sets GL state for upload and copy.
class ScopedUnpackState {
 public:
  explicit ScopedUnpackState(bool uploading_data,
                             int unpack_row_length = 0,
                             int unpack_alignment = 4);

  ScopedUnpackState(const ScopedUnpackState&) = delete;
  ScopedUnpackState& operator=(const ScopedUnpackState&) = delete;

  ~ScopedUnpackState();

 private:
  const raw_ptr<gl::GLApi> api_;

  // Always used if |es3_capable|.
  GLint unpack_buffer_ = 0;

  // Always used when |uploading_data|.
  absl::optional<gl::ScopedPixelStore> unpack_alignment_;

  // Used when |uploading_data_| and (|es3_capable| or
  // |supports_unpack_subimage|).
  absl::optional<gl::ScopedPixelStore> unpack_row_length_;
  absl::optional<gl::ScopedPixelStore> unpack_skip_pixels_;
  absl::optional<gl::ScopedPixelStore> unpack_skip_rows_;

  // Used when |uploading_data| and |es3_capable|.
  absl::optional<gl::ScopedPixelStore> unpack_skip_images_;
  absl::optional<gl::ScopedPixelStore> unpack_image_height_;

  // Used when |desktop_gl|.
  absl::optional<gl::ScopedPixelStore> unpack_swap_bytes_;
  absl::optional<gl::ScopedPixelStore> unpack_lsb_first_;
};

// Common helper functions for GLTextureImageBacking and GLImageBacking.
class GPU_GLES2_EXPORT GLTextureImageBackingHelper {
 public:
  // At destriction time, restore `target`'s binding as of construction time. If
  // `new_binding` is non-zero, then bind `target` to it at construction time.
  // TODO(crbug.com/1367187): Fold into gl::ScopedRestoreTexture.
  class ScopedRestoreTexture {
   public:
    ScopedRestoreTexture(gl::GLApi* api, GLenum target, GLuint new_binding = 0);

    ScopedRestoreTexture(const ScopedRestoreTexture&) = delete;
    ScopedRestoreTexture& operator=(const ScopedRestoreTexture&) = delete;

    ~ScopedRestoreTexture();

   private:
    raw_ptr<gl::GLApi> api_;
    GLenum target_;
    GLuint old_binding_ = 0;
  };

  // Creates a new GL texture and returns GL texture ID.
  static GLuint MakeTextureAndSetParameters(
      GLenum target,
      bool framebuffer_attachment_angle,
      scoped_refptr<gles2::TexturePassthrough>* passthrough_texture,
      raw_ptr<gles2::Texture>* texture);

  // Create a Dawn backing. This will use |backing|'s ProduceGLTexture or
  // ProduceGLTexturePassthrough method, and populate the dawn backing via
  // CopyTextureCHROMIUM.
  static std::unique_ptr<DawnImageRepresentation> ProduceDawnCommon(
      SharedImageFactory* factory,
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type,
      std::vector<WGPUTextureFormat> view_formats,
      SharedImageBacking* backing,
      bool use_passthrough);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_HELPER_H_
