// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_HOLDER_H_

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/texture_holder.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkImageInfo.h"

namespace blink {
class SkiaTextureHolder;

class PLATFORM_EXPORT MailboxTextureHolder final : public TextureHolder {
 public:
  ~MailboxTextureHolder() override;

  // TextureHolder impl.
  IntSize Size() const final {
    return IntSize(sk_image_info_.width(), sk_image_info_.height());
  }
  bool CurrentFrameKnownToBeOpaque() const final { return false; }
  bool IsValid() const final;

  bool IsCrossThread() const;
  const gpu::Mailbox& GetMailbox() const { return mailbox_; }
  const gpu::SyncToken& GetSyncToken() const {
    return mailbox_ref()->sync_token();
  }
  void UpdateSyncToken(gpu::SyncToken sync_token) {
    mailbox_ref()->set_sync_token(sync_token);
  }
  const SkImageInfo& sk_image_info() const { return sk_image_info_; }
  GLenum texture_target() const { return texture_target_; }

  void Sync(MailboxSyncMode);
  // In WebGL's commit or transferToImageBitmap calls, it will call the
  // DrawingBuffer::transferToStaticBitmapImage function, which produces the
  // input parameters for this method.
  MailboxTextureHolder(const gpu::Mailbox&,
                       const gpu::SyncToken&,
                       unsigned texture_id_to_delete_after_mailbox_consumed,
                       base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&,
                       IntSize mailbox_size,
                       bool is_origin_top_left);
  // This function turns a texture-backed SkImage into a mailbox and a
  // syncToken.
  MailboxTextureHolder(const SkiaTextureHolder*, GLenum filter);
  // This function may be used when the MailboxTextureHolder is created on a
  // different thread. The caller must provide a verified sync token if it is
  // created cross-thread.
  MailboxTextureHolder(const gpu::Mailbox&,
                       const gpu::SyncToken&,
                       base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&,
                       scoped_refptr<MailboxRef> mailbox_ref,
                       PlatformThreadId context_thread_id,
                       const SkImageInfo& sk_image_info,
                       GLenum texture_target,
                       bool is_origin_top_left);

 private:
  void InitCommon();

  gpu::Mailbox mailbox_;
  unsigned texture_id_;
  bool is_converted_from_skia_texture_;
  scoped_refptr<base::SingleThreadTaskRunner> texture_thread_task_runner_;
  base::PlatformThreadId thread_id_;
  bool did_issue_ordering_barrier_ = false;
  SkImageInfo sk_image_info_;
  GLenum texture_target_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_HOLDER_H_
