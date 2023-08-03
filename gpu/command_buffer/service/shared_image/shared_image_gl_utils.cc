// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_gl_utils.h"

#include "gpu/command_buffer/service/gl_utils.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"

namespace gpu {

ScopedPackState::ScopedPackState(int pack_row_length, int pack_alignment)
    : api_(gl::g_current_gl_context) {
  bool is_es3_capable = gl::g_current_gl_version->is_es3_capable;

  if (is_es3_capable) {
    // Need to unbind any GL_PIXEL_PACK_BUFFER for the nullptr in
    // glTexImage2D to mean "no pixels" (as opposed to offset 0 in the
    // buffer).
    api_->glGetIntegervFn(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer_);
    if (pack_buffer_) {
      api_->glBindBufferFn(GL_PIXEL_PACK_BUFFER, 0);
    }
  }

  pack_alignment_.emplace(GL_PACK_ALIGNMENT, pack_alignment);

  if (is_es3_capable) {
    pack_row_length_.emplace(GL_PACK_ROW_LENGTH, pack_row_length);
    pack_skip_rows_.emplace(GL_PACK_SKIP_ROWS, 0);
    pack_skip_pixels_.emplace(GL_PACK_SKIP_PIXELS, 0);
  } else {
    DCHECK_EQ(pack_row_length, 0);
  }
}

ScopedPackState::~ScopedPackState() {
  if (pack_buffer_) {
    api_->glBindBufferFn(GL_PIXEL_PACK_BUFFER, pack_buffer_);
  }
}

ScopedUnpackState::ScopedUnpackState(bool uploading_data,
                                     int unpack_row_length,
                                     int unpack_alignment)
    : api_(gl::g_current_gl_context) {
  const auto* version_info = gl::g_current_gl_version;
  bool is_es3_capable = version_info->is_es3_capable;

  if (is_es3_capable) {
    // Need to unbind any GL_PIXEL_UNPACK_BUFFER for the nullptr in
    // glTexImage2D to mean "no pixels" (as opposed to offset 0 in the
    // buffer).
    api_->glGetIntegervFn(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buffer_);
    if (unpack_buffer_) {
      api_->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
    }
  }
  if (uploading_data) {
    unpack_alignment_.emplace(GL_UNPACK_ALIGNMENT, unpack_alignment);

    if (is_es3_capable ||
        gl::g_current_gl_driver->ext.b_GL_EXT_unpack_subimage) {
      unpack_row_length_.emplace(GL_UNPACK_ROW_LENGTH, unpack_row_length);
      unpack_skip_rows_.emplace(GL_UNPACK_SKIP_ROWS, 0);
      unpack_skip_pixels_.emplace(GL_UNPACK_SKIP_PIXELS, 0);
    } else {
      DCHECK_EQ(unpack_row_length, 0);
    }

    if (is_es3_capable) {
      unpack_skip_images_.emplace(GL_UNPACK_SKIP_IMAGES, 0);
      unpack_image_height_.emplace(GL_UNPACK_IMAGE_HEIGHT, 0);
    }

    if (!version_info->is_es) {
      unpack_swap_bytes_.emplace(GL_UNPACK_SWAP_BYTES, GL_FALSE);
      unpack_lsb_first_.emplace(GL_UNPACK_LSB_FIRST, GL_FALSE);
    }
  }
}

ScopedUnpackState::~ScopedUnpackState() {
  if (unpack_buffer_) {
    api_->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, unpack_buffer_);
  }
}

ScopedRestoreTexture::ScopedRestoreTexture(gl::GLApi* api,
                                           GLenum target,
                                           GLuint new_binding)
    : api_(api), target_(target) {
  GLenum get_target = GL_TEXTURE_BINDING_2D;
  switch (target) {
    case GL_TEXTURE_2D:
      get_target = GL_TEXTURE_BINDING_2D;
      break;
    case GL_TEXTURE_RECTANGLE_ARB:
      get_target = GL_TEXTURE_BINDING_RECTANGLE_ARB;
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      get_target = GL_TEXTURE_BINDING_EXTERNAL_OES;
      break;
    default:
      NOTREACHED();
      break;
  }
  GLint old_texture_binding = 0;
  api->glGetIntegervFn(get_target, &old_texture_binding);
  old_binding_ = old_texture_binding;
  if (new_binding) {
    api_->glBindTextureFn(target_, new_binding);
  }
}

ScopedRestoreTexture::~ScopedRestoreTexture() {
  api_->glBindTextureFn(target_, old_binding_);
}

GLuint MakeTextureAndSetParameters(
    GLenum target,
    bool framebuffer_attachment_angle,
    scoped_refptr<gles2::TexturePassthrough>* passthrough_texture,
    raw_ptr<gles2::Texture>* texture) {
  gl::GLApi* api = gl::g_current_gl_context;
  ScopedRestoreTexture scoped_restore(api, target);

  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  api->glBindTextureFn(target, service_id);
  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (framebuffer_attachment_angle) {
    api->glTexParameteriFn(target, GL_TEXTURE_USAGE_ANGLE,
                           GL_FRAMEBUFFER_ATTACHMENT_ANGLE);
  }
  if (passthrough_texture) {
    *passthrough_texture =
        base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
  }
  if (texture) {
    *texture = gles2::CreateGLES2TextureWithLightRef(service_id, target);
  }
  return service_id;
}

}  // namespace gpu
