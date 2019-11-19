// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/skia_texture_holder.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_texture_holder.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

namespace {
bool IsSkImageOriginTopLeft(sk_sp<SkImage> image) {
  GrSurfaceOrigin origin;
  image->getBackendTexture(false, &origin);
  return origin == kTopLeft_GrSurfaceOrigin;
}

struct ReleaseContext {
  scoped_refptr<TextureHolder::MailboxRef> mailbox_ref;
  GLuint texture_id = 0u;
  bool is_shared_image = false;
  GrTexture* gr_texture = nullptr;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper;
};

void ReleaseTexture(void* ctx) {
  auto* release_ctx = static_cast<ReleaseContext*>(ctx);
  if (release_ctx->context_provider_wrapper) {
    if (release_ctx->gr_texture) {
      release_ctx->context_provider_wrapper->Utils()->RemoveCachedMailbox(
          release_ctx->gr_texture);
    }

    if (release_ctx->texture_id) {
      auto* gl =
          release_ctx->context_provider_wrapper->ContextProvider()->ContextGL();
      if (release_ctx->is_shared_image)
        gl->EndSharedImageAccessDirectCHROMIUM(release_ctx->texture_id);
      gl->DeleteTextures(1u, &release_ctx->texture_id);
    }
  }

  delete release_ctx;
}

}  // namespace

SkiaTextureHolder::SkiaTextureHolder(
    sk_sp<SkImage> image,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper)
    : TextureHolder(std::move(context_provider_wrapper),
                    base::MakeRefCounted<MailboxRef>(nullptr),
                    IsSkImageOriginTopLeft(image)),
      image_(std::move(image)) {}

SkiaTextureHolder::SkiaTextureHolder(
    const MailboxTextureHolder* mailbox_texture_holder,
    GLuint shared_image_texture_id)
    : TextureHolder(SharedGpuContext::ContextProviderWrapper(),
                    mailbox_texture_holder->mailbox_ref(),
                    mailbox_texture_holder->IsOriginTopLeft()) {
  const gpu::Mailbox mailbox = mailbox_texture_holder->GetMailbox();
  DCHECK(!shared_image_texture_id || mailbox.IsSharedImage());
  DCHECK(!shared_image_texture_id || !mailbox_texture_holder->IsCrossThread());

  const gpu::SyncToken sync_token = mailbox_texture_holder->GetSyncToken();
  const auto& sk_image_info = mailbox_texture_holder->sk_image_info();
  GLenum texture_target = mailbox_texture_holder->texture_target();

  if (!ContextProvider())
    return;

  gpu::gles2::GLES2Interface* shared_gl = ContextProvider()->ContextGL();
  GrContext* shared_gr_context = ContextProvider()->GetGrContext();
  DCHECK(shared_gl &&
         shared_gr_context);  // context isValid already checked in callers

  GLuint shared_context_texture_id = 0u;
  bool should_delete_texture_on_release = true;

  if (shared_image_texture_id) {
    shared_context_texture_id = shared_image_texture_id;
    should_delete_texture_on_release = false;
  } else {
    shared_gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    if (mailbox.IsSharedImage()) {
      shared_context_texture_id =
          shared_gl->CreateAndTexStorage2DSharedImageCHROMIUM(mailbox.name);
      shared_gl->BeginSharedImageAccessDirectCHROMIUM(
          shared_context_texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    } else {
      shared_context_texture_id =
          shared_gl->CreateAndConsumeTextureCHROMIUM(mailbox.name);
    }
  }

  GrGLTextureInfo texture_info;
  texture_info.fTarget = texture_target;
  texture_info.fID = shared_context_texture_id;
  texture_info.fFormat =
      CanvasColorParams(sk_image_info).GLSizedInternalFormat();
  GrBackendTexture backend_texture(sk_image_info.width(),
                                   sk_image_info.height(), GrMipMapped::kNo,
                                   texture_info);

  GrSurfaceOrigin origin = IsOriginTopLeft() ? kTopLeft_GrSurfaceOrigin
                                             : kBottomLeft_GrSurfaceOrigin;

  auto* release_ctx = new ReleaseContext;
  release_ctx->mailbox_ref = mailbox_ref();
  if (should_delete_texture_on_release)
    release_ctx->texture_id = shared_context_texture_id;
  release_ctx->is_shared_image = mailbox.IsSharedImage();
  release_ctx->context_provider_wrapper = ContextProviderWrapper();

  image_ = SkImage::MakeFromTexture(
      shared_gr_context, backend_texture, origin, sk_image_info.colorType(),
      sk_image_info.alphaType(), sk_image_info.refColorSpace(), &ReleaseTexture,
      release_ctx);
  if (image_) {
    release_ctx->gr_texture = image_->getTexture();
    DCHECK(release_ctx->gr_texture);
    ContextProviderWrapper()->Utils()->RegisterMailbox(image_->getTexture(),
                                                       mailbox);
  } else {
    ReleaseTexture(release_ctx);
  }
}

SkiaTextureHolder::~SkiaTextureHolder() {
  // Object must be destroyed on the same thread where it was created.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool SkiaTextureHolder::IsValid() const {
  return !!image_ && !!ContextProviderWrapper() &&
         image_->isValid(ContextProvider()->GetGrContext());
}

}  // namespace blink
