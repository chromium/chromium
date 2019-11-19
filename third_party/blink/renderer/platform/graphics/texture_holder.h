// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEXTURE_HOLDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEXTURE_HOLDER_H_

#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT TextureHolder {
 public:
  class MailboxRef : public ThreadSafeRefCounted<MailboxRef> {
   public:
    explicit MailboxRef(
        std::unique_ptr<viz::SingleReleaseCallback> release_callback)
        : release_callback_(std::move(release_callback)) {}
    ~MailboxRef() {
      if (release_callback_)
        release_callback_->Run(sync_token_, false /* resource_lost */);
    }

    void set_sync_token(gpu::SyncToken token) { sync_token_ = token; }
    const gpu::SyncToken& sync_token() const { return sync_token_; }

   private:
    gpu::SyncToken sync_token_;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback_;
  };

  virtual ~TextureHolder() = default;

  // Methods overridden by all sub-classes
  virtual IntSize Size() const = 0;
  virtual bool CurrentFrameKnownToBeOpaque() const = 0;
  virtual bool IsValid() const = 0;

  // Methods that have exactly the same impelmentation for all sub-classes
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const {
    return context_provider_wrapper_;
  }

  WebGraphicsContext3DProvider* ContextProvider() const {
    return context_provider_wrapper_
               ? context_provider_wrapper_->ContextProvider()
               : nullptr;
  }

  bool IsOriginTopLeft() const { return is_origin_top_left_; }

  const scoped_refptr<MailboxRef>& mailbox_ref() const { return mailbox_ref_; }

 protected:
  TextureHolder(base::WeakPtr<WebGraphicsContext3DProviderWrapper>&&
                    context_provider_wrapper,
                scoped_refptr<MailboxRef> mailbox_ref,
                bool is_origin_top_left)
      : context_provider_wrapper_(std::move(context_provider_wrapper)),
        mailbox_ref_(std::move(mailbox_ref)),
        is_origin_top_left_(is_origin_top_left) {
    DCHECK(mailbox_ref_);
  }

 private:
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  scoped_refptr<MailboxRef> mailbox_ref_;
  bool is_origin_top_left_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_TEXTURE_HOLDER_H_
