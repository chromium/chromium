// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_GL_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_GL_UTILS_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_utils.h"

namespace gpu {

// Common utilities for various backings.

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
  std::optional<gl::ScopedPixelStore> pack_alignment_;
  std::optional<gl::ScopedPixelStore> pack_row_length_;
  std::optional<gl::ScopedPixelStore> pack_skip_pixels_;
  std::optional<gl::ScopedPixelStore> pack_skip_rows_;
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
  std::optional<gl::ScopedPixelStore> unpack_alignment_;

  // Used when |uploading_data_| and (|es3_capable| or
  // |supports_unpack_subimage|).
  std::optional<gl::ScopedPixelStore> unpack_row_length_;
  std::optional<gl::ScopedPixelStore> unpack_skip_pixels_;
  std::optional<gl::ScopedPixelStore> unpack_skip_rows_;

  // Used when |uploading_data| and |es3_capable|.
  std::optional<gl::ScopedPixelStore> unpack_skip_images_;
  std::optional<gl::ScopedPixelStore> unpack_image_height_;

  // Used when |desktop_gl|.
  std::optional<gl::ScopedPixelStore> unpack_swap_bytes_;
  std::optional<gl::ScopedPixelStore> unpack_lsb_first_;
};

// Creates a new GL texture and returns GL texture ID.
GLuint MakeTextureAndSetParameters(
    GLenum target,
    bool framebuffer_attachment_angle,
    scoped_refptr<gles2::TexturePassthrough>* passthrough_texture,
    raw_ptr<gles2::Texture>* texture);

// Returns whether `glTexStorage2DEXTFn` is available to call on the GL driver.
bool IsTexStorage2DAvailable();

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SHARED_IMAGE_GL_UTILS_H_
