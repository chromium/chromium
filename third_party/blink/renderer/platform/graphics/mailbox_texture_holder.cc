// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/mailbox_texture_holder.h"

#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia_texture_holder.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace blink {

namespace {

void ReleaseTexture(
    bool is_converted_from_skia_texture,
    unsigned texture_id,
    std::unique_ptr<gpu::Mailbox> mailbox,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider,
    std::unique_ptr<gpu::SyncToken> sync_token) {
  if (!is_converted_from_skia_texture && texture_id && context_provider) {
    context_provider->ContextProvider()->ContextGL()->WaitSyncTokenCHROMIUM(
        sync_token->GetData());
    context_provider->ContextProvider()->ContextGL()->DeleteTextures(
        1, &texture_id);
  }
}

}  // namespace

MailboxTextureHolder::MailboxTextureHolder(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    unsigned texture_id_to_delete_after_mailbox_consumed,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper,
    IntSize mailbox_size,
    bool is_origin_top_left)
    : TextureHolder(std::move(context_provider_wrapper),
                    base::MakeRefCounted<MailboxRef>(nullptr),
                    is_origin_top_left),
      mailbox_(mailbox),
      texture_id_(texture_id_to_delete_after_mailbox_consumed),
      is_converted_from_skia_texture_(false),
      thread_id_(0),
      sk_image_info_(SkImageInfo::MakeN32Premul(mailbox_size.Width(),
                                                mailbox_size.Height())),
      texture_target_(GL_TEXTURE_2D) {
  mailbox_ref()->set_sync_token(sync_token);
  InitCommon();
}

MailboxTextureHolder::MailboxTextureHolder(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
        context_provider_wrapper,
    scoped_refptr<MailboxRef> mailbox_ref,
    PlatformThreadId context_thread_id,
    const SkImageInfo& sk_image_info,
    GLenum texture_target,
    bool is_origin_top_left)
    : TextureHolder(std::move(context_provider_wrapper),
                    std::move(mailbox_ref),
                    is_origin_top_left),
      mailbox_(mailbox),
      texture_id_(0),
      is_converted_from_skia_texture_(false),
      thread_id_(context_thread_id),
      sk_image_info_(sk_image_info),
      texture_target_(texture_target) {
  DCHECK(thread_id_);
  DCHECK(!IsCrossThread() || sync_token.verified_flush());
  this->mailbox_ref()->set_sync_token(sync_token);
}

MailboxTextureHolder::MailboxTextureHolder(
    const SkiaTextureHolder* texture_holder,
    GLenum filter)
    : TextureHolder(texture_holder->ContextProviderWrapper(),
                    texture_holder->mailbox_ref(),
                    texture_holder->IsOriginTopLeft()),
      texture_id_(0),
      is_converted_from_skia_texture_(true),
      thread_id_(0) {
  sk_sp<SkImage> image = texture_holder->GetSkImage();
  DCHECK(image);
  sk_image_info_ = image->imageInfo();

  if (!ContextProviderWrapper())
    return;

  if (!ContextProviderWrapper()->Utils()->GetMailboxForSkImage(
          mailbox_, texture_target_, image, filter))
    return;

  InitCommon();
}

void MailboxTextureHolder::Sync(MailboxSyncMode mode) {
  gpu::SyncToken sync_token = mailbox_ref()->sync_token();

  if (IsCrossThread()) {
    // Was originally created on another thread. Should already have a sync
    // token from the original source context, already verified if needed.
    DCHECK(sync_token.HasData());
    DCHECK(mode != kVerifiedSyncToken || sync_token.verified_flush());
    return;
  }

  if (!ContextProviderWrapper())
    return;

  TRACE_EVENT0("blink", "MailboxTextureHolder::Sync");

  gpu::gles2::GLES2Interface* gl =
      ContextProviderWrapper()->ContextProvider()->ContextGL();

  if (mode == kOrderingBarrier) {
    if (!did_issue_ordering_barrier_) {
      gl->OrderingBarrierCHROMIUM();
      did_issue_ordering_barrier_ = true;
    }
    return;
  }

  if (!sync_token.HasData()) {
    if (mode == kVerifiedSyncToken) {
      gl->GenSyncTokenCHROMIUM(sync_token.GetData());
    } else {
      gl->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
    }
    mailbox_ref()->set_sync_token(sync_token);
    return;
  }

  // At this point we have a pre-existing sync token. We just need to verify
  // it if needed.  Providing a verified sync token when unverified is requested
  // is fine.
  if (mode == kVerifiedSyncToken && !sync_token.verified_flush()) {
    int8_t* token_data = sync_token.GetData();
    // TODO(junov): Batch this verification in the case where there are multiple
    // offscreen canvases being committed.
    gl->ShallowFlushCHROMIUM();
    gl->VerifySyncTokensCHROMIUM(&token_data, 1);
    sync_token.SetVerifyFlush();
    mailbox_ref()->set_sync_token(sync_token);
  }
}

void MailboxTextureHolder::InitCommon() {
  DCHECK(!thread_id_);
  thread_id_ = base::PlatformThread::CurrentId();
  texture_thread_task_runner_ = Thread::Current()->GetTaskRunner();
}

bool MailboxTextureHolder::IsValid() const {
  if (IsCrossThread()) {
    // If context is is from another thread, validity cannot be verified.
    // Just assume valid. Potential problem will be detected later.
    return true;
  }
  return !!ContextProviderWrapper();
}

bool MailboxTextureHolder::IsCrossThread() const {
  return thread_id_ != base::PlatformThread::CurrentId();
}

MailboxTextureHolder::~MailboxTextureHolder() {
  std::unique_ptr<gpu::SyncToken> passed_sync_token(
      new gpu::SyncToken(mailbox_ref()->sync_token()));
  std::unique_ptr<gpu::Mailbox> passed_mailbox(new gpu::Mailbox(mailbox_));

  if (texture_thread_task_runner_ && IsCrossThread()) {
    PostCrossThreadTask(
        *texture_thread_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&ReleaseTexture, is_converted_from_skia_texture_,
                            texture_id_, WTF::Passed(std::move(passed_mailbox)),
                            WTF::Passed(ContextProviderWrapper()),
                            WTF::Passed(std::move(passed_sync_token))));
  } else {
    ReleaseTexture(is_converted_from_skia_texture_, texture_id_,
                   std::move(passed_mailbox), ContextProviderWrapper(),
                   std::move(passed_sync_token));
  }

  texture_id_ = 0u;  // invalidate the texture.
  texture_thread_task_runner_ = nullptr;
}

}  // namespace blink
