// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_HELPER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "ui/gl/gl_bindings.h"

namespace gpu {

// Common helper functions for GLTextureImageBacking and GLImageBacking.
class GPU_GLES2_EXPORT GLTextureImageBackingHelper {
 public:
  // These parameters are used to explicitly initialize a GL texture.
  struct InitializeGLTextureParams {
    GLenum target = 0;
    GLenum internal_format = 0;
    GLenum format = 0;
    GLenum type = 0;
    bool is_cleared = false;
    bool is_rgb_emulation = false;
    bool framebuffer_attachment_angle = false;
    bool has_immutable_storage = false;
  };

  // Attributes needed to know what state to restore for GL upload and copy.
  struct UnpackStateAttribs {
    bool es3_capable = false;
    bool desktop_gl = false;
    bool supports_unpack_subimage = false;
  };

  // Object used to restore state around GL upload and copy.
  class ScopedResetAndRestoreUnpackState {
   public:
    ScopedResetAndRestoreUnpackState(gl::GLApi* api,
                                     const UnpackStateAttribs& attribs,
                                     bool uploading_data);

    ScopedResetAndRestoreUnpackState(const ScopedResetAndRestoreUnpackState&) =
        delete;
    ScopedResetAndRestoreUnpackState& operator=(
        const ScopedResetAndRestoreUnpackState&) = delete;

    ~ScopedResetAndRestoreUnpackState();

   private:
    const raw_ptr<gl::GLApi> api_;

    // Always used if |es3_capable|.
    GLint unpack_buffer_ = 0;

    // Always used when |uploading_data|.
    GLint unpack_alignment_ = 4;

    // Used when |uploading_data_| and (|es3_capable| or
    // |supports_unpack_subimage|).
    GLint unpack_row_length_ = 0;
    GLint unpack_skip_pixels_ = 0;
    GLint unpack_skip_rows_ = 0;

    // Used when |uploading_data| and |es3_capable|.
    GLint unpack_skip_images_ = 0;
    GLint unpack_image_height_ = 0;

    // Used when |desktop_gl|.
    GLboolean unpack_swap_bytes_ = GL_FALSE;
    GLboolean unpack_lsb_first_ = GL_FALSE;
  };

  // Object used to restore texture bindings.
  class ScopedRestoreTexture {
   public:
    ScopedRestoreTexture(gl::GLApi* api, GLenum target);

    ScopedRestoreTexture(const ScopedRestoreTexture&) = delete;
    ScopedRestoreTexture& operator=(const ScopedRestoreTexture&) = delete;

    ~ScopedRestoreTexture();

   private:
    raw_ptr<gl::GLApi> api_;
    GLenum target_;
    GLuint old_binding_ = 0;
  };

  // Helper function to create a GL texture.
  static void MakeTextureAndSetParameters(
      GLenum target,
      GLuint service_id,
      bool framebuffer_attachment_angle,
      scoped_refptr<gles2::TexturePassthrough>* passthrough_texture,
      gles2::Texture** texture);

  // Create a Dawn backing. This will use |backing|'s ProduceGLTexture or
  // ProduceGLTexturePassthrough method, and populate the dawn backing via
  // CopyTextureCHROMIUM.
  static std::unique_ptr<DawnImageRepresentation> ProduceDawnCommon(
      SharedImageFactory* factory,
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type,
      SharedImageBacking* backing,
      bool use_passthrough);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_GL_TEXTURE_IMAGE_BACKING_HELPER_H_
