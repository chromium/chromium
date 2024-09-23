// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_gl_utils.h"

#include "gpu/command_buffer/service/gl_utils.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_restore_texture.h"

namespace gpu {

ScopedPackState::ScopedPackState(int pack_row_length, int pack_alignment)
    : api_(gl::g_current_gl_context) {
  bool is_es3 = gl::g_current_gl_version->IsAtLeastGLES(3, 0);

  if (is_es3) {
    // Need to unbind any GL_PIXEL_PACK_BUFFER for the nullptr in
    // glTexImage2D to mean "no pixels" (as opposed to offset 0 in the
    // buffer).
    api_->glGetIntegervFn(GL_PIXEL_PACK_BUFFER_BINDING, &pack_buffer_);
    if (pack_buffer_) {
      api_->glBindBufferFn(GL_PIXEL_PACK_BUFFER, 0);
    }
  }

  pack_alignment_.emplace(GL_PACK_ALIGNMENT, pack_alignment);

  if (is_es3) {
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
  bool is_es3 = version_info->IsAtLeastGLES(3, 0);

  if (is_es3) {
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

    if (is_es3 || gl::g_current_gl_driver->ext.b_GL_EXT_unpack_subimage) {
      unpack_row_length_.emplace(GL_UNPACK_ROW_LENGTH, unpack_row_length);
      unpack_skip_rows_.emplace(GL_UNPACK_SKIP_ROWS, 0);
      unpack_skip_pixels_.emplace(GL_UNPACK_SKIP_PIXELS, 0);
    } else {
      DCHECK_EQ(unpack_row_length, 0);
    }

    if (is_es3) {
      unpack_skip_images_.emplace(GL_UNPACK_SKIP_IMAGES, 0);
      unpack_image_height_.emplace(GL_UNPACK_IMAGE_HEIGHT, 0);
    }
  }
}

ScopedUnpackState::~ScopedUnpackState() {
  if (unpack_buffer_) {
    api_->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, unpack_buffer_);
  }
}

GLuint MakeTextureAndSetParameters(
    GLenum target,
    bool framebuffer_attachment_angle,
    scoped_refptr<gles2::TexturePassthrough>* passthrough_texture,
    raw_ptr<gles2::Texture>* texture) {
  gl::GLApi* api = gl::g_current_gl_context;
  gl::ScopedRestoreTexture scoped_restore(api, target);

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

bool IsTexStorage2DAvailable() {
  const auto* version_info = gl::g_current_gl_version;
  const auto& ext = gl::g_current_gl_driver->ext;
  return ext.b_GL_EXT_texture_storage || version_info->is_es3;
}

}  // namespace gpu
