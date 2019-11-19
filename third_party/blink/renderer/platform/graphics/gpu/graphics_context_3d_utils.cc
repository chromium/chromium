// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/graphics_context_3d_utils.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace {

struct GrTextureMailboxReleaseProcData {
  GrTexture* gr_texture_;
  base::WeakPtr<blink::WebGraphicsContext3DProviderWrapper>
      context_provider_wrapper_;
};

void GrTextureMailboxReleaseProc(void* data) {
  GrTextureMailboxReleaseProcData* release_proc_data =
      static_cast<GrTextureMailboxReleaseProcData*>(data);

  if (release_proc_data->context_provider_wrapper_) {
    release_proc_data->context_provider_wrapper_->Utils()->RemoveCachedMailbox(
        release_proc_data->gr_texture_);
  }

  delete release_proc_data;
}

}  // unnamed namespace

namespace blink {

bool GraphicsContext3DUtils::GetMailboxForSkImage(gpu::Mailbox& out_mailbox,
                                                  GLenum& out_texture_target,
                                                  const sk_sp<SkImage>& image,
                                                  GLenum filter) {
  // This object is owned by context_provider_wrapper_, so that weak ref
  // should never be null.
  DCHECK(context_provider_wrapper_);
  DCHECK(image->isTextureBacked());
  GrContext* gr = context_provider_wrapper_->ContextProvider()->GetGrContext();
  gpu::gles2::GLES2Interface* gl =
      context_provider_wrapper_->ContextProvider()->ContextGL();

  DCHECK(gr);
  DCHECK(gl);
  GrTexture* gr_texture = image->getTexture();
  if (!gr_texture)
    return false;

  DCHECK(gr == gr_texture->getContext());

  GrBackendTexture backend_texture = image->getBackendTexture(true);
  DCHECK(backend_texture.isValid());

  GrGLTextureInfo info;
  bool result = backend_texture.getGLTextureInfo(&info);
  DCHECK(result);

  GLuint texture_id = info.fID;
  GLenum texture_target = info.fTarget;
  out_texture_target = texture_target;

  auto it = cached_mailboxes_.find(gr_texture);
  if (it != cached_mailboxes_.end()) {
    out_mailbox = it->value;
  } else {
    gl->ProduceTextureDirectCHROMIUM(texture_id, out_mailbox.name);

    GrTextureMailboxReleaseProcData* release_proc_data =
        new GrTextureMailboxReleaseProcData();
    release_proc_data->gr_texture_ = gr_texture;
    release_proc_data->context_provider_wrapper_ = context_provider_wrapper_;
    gr_texture->setRelease(GrTextureMailboxReleaseProc, release_proc_data);
    cached_mailboxes_.insert(gr_texture, out_mailbox);
  }
  gl->BindTexture(texture_target, texture_id);
  gl->TexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, filter);
  gl->TexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, filter);
  gl->TexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->TexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->BindTexture(texture_target, 0);
  gr_texture->textureParamsModified();
  return true;
}

void GraphicsContext3DUtils::RegisterMailbox(GrTexture* gr_texture,
                                             const gpu::Mailbox& mailbox) {
  DCHECK(cached_mailboxes_.find(gr_texture) == cached_mailboxes_.end());
  cached_mailboxes_.insert(gr_texture, mailbox);
}

void GraphicsContext3DUtils::RemoveCachedMailbox(GrTexture* gr_texture) {
  cached_mailboxes_.erase(gr_texture);
}

bool GraphicsContext3DUtils::Accelerated2DCanvasFeatureEnabled() {
  // Don't use accelerated canvas if compositor is in software mode.
  if (!SharedGpuContext::IsGpuCompositingEnabled())
    return false;

  if (!RuntimeEnabledFeatures::Accelerated2dCanvasEnabled())
    return false;

  DCHECK(context_provider_wrapper_);
  const gpu::GpuFeatureInfo& gpu_feature_info =
      context_provider_wrapper_->ContextProvider()->GetGpuFeatureInfo();
  return gpu::kGpuFeatureStatusEnabled ==
         gpu_feature_info
             .status_values[gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS];
}

}  // namespace blink
