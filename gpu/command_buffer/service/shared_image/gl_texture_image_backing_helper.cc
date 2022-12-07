// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_helper.h"

#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "ui/gl/gl_context.h"
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
    if (pack_buffer_)
      api_->glBindBufferFn(GL_PIXEL_PACK_BUFFER, 0);
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
  if (pack_buffer_)
    api_->glBindBufferFn(GL_PIXEL_PACK_BUFFER, pack_buffer_);
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
    if (unpack_buffer_)
      api_->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
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
  if (unpack_buffer_)
    api_->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, unpack_buffer_);
}

GLTextureImageBackingHelper::ScopedRestoreTexture::ScopedRestoreTexture(
    gl::GLApi* api,
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
  if (new_binding)
    api_->glBindTextureFn(target_, new_binding);
}

GLTextureImageBackingHelper::ScopedRestoreTexture::~ScopedRestoreTexture() {
  api_->glBindTextureFn(target_, old_binding_);
}

std::unique_ptr<DawnImageRepresentation>
GLTextureImageBackingHelper::ProduceDawnCommon(
    SharedImageFactory* factory,
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats,
    SharedImageBacking* backing,
    bool use_passthrough) {
  DCHECK(factory);
  // Make SharedContextState from factory the current context
  SharedContextState* shared_context_state = factory->GetSharedContextState();
  if (!shared_context_state->MakeCurrent(nullptr, true)) {
    DLOG(ERROR) << "Cannot make util SharedContextState the current context";
    return nullptr;
  }

  Mailbox dst_mailbox = Mailbox::GenerateForSharedImage();

  bool success = factory->CreateSharedImage(
      dst_mailbox, backing->format(), backing->size(), backing->color_space(),
      kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType, gpu::kNullSurfaceHandle,
      backing->usage() | SHARED_IMAGE_USAGE_WEBGPU);
  if (!success) {
    DLOG(ERROR) << "Cannot create a shared image resource for internal blit";
    return nullptr;
  }

  // Create a representation for current backing to avoid non-expected release
  // and using scope access methods.
  std::unique_ptr<GLTextureImageRepresentationBase> src_image;
  std::unique_ptr<GLTextureImageRepresentationBase> dst_image;
  if (use_passthrough) {
    src_image =
        manager->ProduceGLTexturePassthrough(backing->mailbox(), tracker);
    dst_image = manager->ProduceGLTexturePassthrough(dst_mailbox, tracker);
  } else {
    src_image = manager->ProduceGLTexture(backing->mailbox(), tracker);
    dst_image = manager->ProduceGLTexture(dst_mailbox, tracker);
  }

  if (!src_image || !dst_image) {
    DLOG(ERROR) << "ProduceDawn: Couldn't produce shared image for copy";
    return nullptr;
  }

  std::unique_ptr<GLTextureImageRepresentationBase::ScopedAccess>
      source_access = src_image->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kNo);
  if (!source_access) {
    DLOG(ERROR) << "ProduceDawn: Couldn't access shared image for copy.";
    return nullptr;
  }

  std::unique_ptr<GLTextureImageRepresentationBase::ScopedAccess> dest_access =
      dst_image->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kYes);
  if (!dest_access) {
    DLOG(ERROR) << "ProduceDawn: Couldn't access shared image for copy.";
    return nullptr;
  }

  GLuint source_texture = src_image->GetTextureBase()->service_id();
  GLuint dest_texture = dst_image->GetTextureBase()->service_id();
  DCHECK_NE(source_texture, dest_texture);

  GLenum target = dst_image->GetTextureBase()->target();

  // Ensure skia's internal cache of GL context state is reset before using it.
  // TODO(crbug.com/1036142): Figure out cases that need this invocation.
  shared_context_state->PessimisticallyResetGrContext();

  if (use_passthrough) {
    gl::GLApi* gl = shared_context_state->context_state()->api();

    gl->glCopySubTextureCHROMIUMFn(source_texture, 0, target, dest_texture, 0,
                                   0, 0, 0, 0, dst_image->size().width(),
                                   dst_image->size().height(), false, false,
                                   false);
  } else {
    // TODO(crbug.com/1036142): Implement copyTextureCHROMIUM for validating
    // path.
    NOTREACHED();
    return nullptr;
  }

  // Set cleared flag for internal backing to prevent auto clear.
  dst_image->SetCleared();

  // Safe to destroy factory's ref. The backing is kept alive by GL
  // representation ref.
  factory->DestroySharedImage(dst_mailbox);

  return manager->ProduceDawn(dst_mailbox, tracker, device, backend_type,
                              std::move(view_formats));
}

// static
void GLTextureImageBackingHelper::MakeTextureAndSetParameters(
    GLenum target,
    GLuint service_id,
    bool framebuffer_attachment_angle,
    scoped_refptr<gles2::TexturePassthrough>* passthrough_texture,
    gles2::Texture** texture) {
  if (!service_id) {
    gl::GLApi* api = gl::g_current_gl_context;
    ScopedRestoreTexture scoped_restore(api, target);

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
  }
  if (passthrough_texture) {
    *passthrough_texture =
        base::MakeRefCounted<gles2::TexturePassthrough>(service_id, target);
  }
  if (texture) {
    *texture = gles2::CreateGLES2TextureWithLightRef(service_id, target);
  }
}

}  // namespace gpu
