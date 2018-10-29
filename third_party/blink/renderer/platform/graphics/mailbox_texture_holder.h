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

namespace blink {

class PLATFORM_EXPORT MailboxTextureHolder final : public TextureHolder {
 public:
  ~MailboxTextureHolder() override;

  bool IsSkiaTextureHolder() final { return false; }
  bool IsMailboxTextureHolder() final { return true; }
  IntSize Size() const final { return size_; }
  bool CurrentFrameKnownToBeOpaque() final { return false; }
  bool IsValid() const final;
  bool IsCrossThread() const final;

  const gpu::Mailbox& GetMailbox() const final { return mailbox_; }
  const gpu::SyncToken& GetSyncToken() const final { return sync_token_; }
  void UpdateSyncToken(gpu::SyncToken sync_token) final {
    sync_token_ = sync_token;
  }

  void Sync(MailboxSyncMode) final;
  // In WebGL's commit or transferToImageBitmap calls, it will call the
  // DrawingBuffer::transferToStaticBitmapImage function, which produces the
  // input parameters for this method.
  MailboxTextureHolder(const gpu::Mailbox&,
                       const gpu::SyncToken&,
                       unsigned texture_id_to_delete_after_mailbox_consumed,
                       base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&,
                       IntSize mailbox_size);
  // This function turns a texture-backed SkImage into a mailbox and a
  // syncToken.
  MailboxTextureHolder(std::unique_ptr<TextureHolder>, GLenum filter);

 private:
  void InitCommon();

  gpu::Mailbox mailbox_;
  gpu::SyncToken sync_token_;
  unsigned texture_id_;
  IntSize size_;
  bool is_converted_from_skia_texture_;
  scoped_refptr<base::SingleThreadTaskRunner> texture_thread_task_runner_;
  PlatformThreadId thread_id_;
  bool did_issue_ordering_barrier_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_HOLDER_H_
