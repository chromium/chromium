// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_gl_common.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/context_state.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "ui/gl/gl_gl_api_implementation.h"

namespace gpu {

SharedImageBackingGLCommon::ScopedResetAndRestoreUnpackState::
    ScopedResetAndRestoreUnpackState(gl::GLApi* api,
                                     const UnpackStateAttribs& attribs,
                                     bool uploading_data)
    : api_(api) {
  if (attribs.es3_capable) {
    // Need to unbind any GL_PIXEL_UNPACK_BUFFER for the nullptr in
    // glTexImage2D to mean "no pixels" (as opposed to offset 0 in the
    // buffer).
    api_->glGetIntegervFn(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buffer_);
    if (unpack_buffer_)
      api_->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, 0);
  }
  if (uploading_data) {
    api_->glGetIntegervFn(GL_UNPACK_ALIGNMENT, &unpack_alignment_);
    if (unpack_alignment_ != 4)
      api_->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, 4);

    if (attribs.es3_capable || attribs.supports_unpack_subimage) {
      api_->glGetIntegervFn(GL_UNPACK_ROW_LENGTH, &unpack_row_length_);
      if (unpack_row_length_)
        api_->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, 0);
      api_->glGetIntegervFn(GL_UNPACK_SKIP_ROWS, &unpack_skip_rows_);
      if (unpack_skip_rows_)
        api_->glPixelStoreiFn(GL_UNPACK_SKIP_ROWS, 0);
      api_->glGetIntegervFn(GL_UNPACK_SKIP_PIXELS, &unpack_skip_pixels_);
      if (unpack_skip_pixels_)
        api_->glPixelStoreiFn(GL_UNPACK_SKIP_PIXELS, 0);
    }

    if (attribs.es3_capable) {
      api_->glGetIntegervFn(GL_UNPACK_SKIP_IMAGES, &unpack_skip_images_);
      if (unpack_skip_images_)
        api_->glPixelStoreiFn(GL_UNPACK_SKIP_IMAGES, 0);
      api_->glGetIntegervFn(GL_UNPACK_IMAGE_HEIGHT, &unpack_image_height_);
      if (unpack_image_height_)
        api_->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, 0);
    }

    if (attribs.desktop_gl) {
      api->glGetBooleanvFn(GL_UNPACK_SWAP_BYTES, &unpack_swap_bytes_);
      if (unpack_swap_bytes_)
        api->glPixelStoreiFn(GL_UNPACK_SWAP_BYTES, GL_FALSE);
      api->glGetBooleanvFn(GL_UNPACK_LSB_FIRST, &unpack_lsb_first_);
      if (unpack_lsb_first_)
        api->glPixelStoreiFn(GL_UNPACK_LSB_FIRST, GL_FALSE);
    }
  }
}

SharedImageBackingGLCommon::ScopedResetAndRestoreUnpackState::
    ~ScopedResetAndRestoreUnpackState() {
  if (unpack_buffer_)
    api_->glBindBufferFn(GL_PIXEL_UNPACK_BUFFER, unpack_buffer_);
  if (unpack_alignment_ != 4)
    api_->glPixelStoreiFn(GL_UNPACK_ALIGNMENT, unpack_alignment_);
  if (unpack_row_length_)
    api_->glPixelStoreiFn(GL_UNPACK_ROW_LENGTH, unpack_row_length_);
  if (unpack_image_height_)
    api_->glPixelStoreiFn(GL_UNPACK_IMAGE_HEIGHT, unpack_image_height_);
  if (unpack_skip_rows_)
    api_->glPixelStoreiFn(GL_UNPACK_SKIP_ROWS, unpack_skip_rows_);
  if (unpack_skip_images_)
    api_->glPixelStoreiFn(GL_UNPACK_SKIP_IMAGES, unpack_skip_images_);
  if (unpack_skip_pixels_)
    api_->glPixelStoreiFn(GL_UNPACK_SKIP_PIXELS, unpack_skip_pixels_);
  if (unpack_swap_bytes_)
    api_->glPixelStoreiFn(GL_UNPACK_SWAP_BYTES, unpack_swap_bytes_);
  if (unpack_lsb_first_)
    api_->glPixelStoreiFn(GL_UNPACK_LSB_FIRST, unpack_lsb_first_);
}

SharedImageBackingGLCommon::ScopedRestoreTexture::ScopedRestoreTexture(
    gl::GLApi* api,
    GLenum target)
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
}

SharedImageBackingGLCommon::ScopedRestoreTexture::~ScopedRestoreTexture() {
  api_->glBindTextureFn(target_, old_binding_);
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingGLCommon::ProduceDawnCommon(SharedImageFactory* factory,
                                              SharedImageManager* manager,
                                              MemoryTypeTracker* tracker,
                                              WGPUDevice device,
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
  std::unique_ptr<SharedImageRepresentationGLTextureBase> src_image;
  std::unique_ptr<SharedImageRepresentationGLTextureBase> dst_image;
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

  std::unique_ptr<SharedImageRepresentationGLTextureBase::ScopedAccess>
      source_access = src_image->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kNo);
  if (!source_access) {
    DLOG(ERROR) << "ProduceDawn: Couldn't access shared image for copy.";
    return nullptr;
  }

  std::unique_ptr<SharedImageRepresentationGLTextureBase::ScopedAccess>
      dest_access = dst_image->BeginScopedAccess(
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

  return manager->ProduceDawn(dst_mailbox, tracker, device);
}

// static
void SharedImageBackingGLCommon::MakeTextureAndSetParameters(
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
    *texture = new gles2::Texture(service_id);
    (*texture)->SetLightweightRef();
    (*texture)->SetTarget(target, 1);
    (*texture)->set_min_filter(GL_LINEAR);
    (*texture)->set_mag_filter(GL_LINEAR);
    (*texture)->set_wrap_s(GL_CLAMP_TO_EDGE);
    (*texture)->set_wrap_t(GL_CLAMP_TO_EDGE);
  }
}

}  // namespace gpu
