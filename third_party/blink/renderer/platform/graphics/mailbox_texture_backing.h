// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_BACKING_H_

#include "base/memory/weak_ptr.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/mailbox.h"

namespace blink {
class WebGraphicsContext3DProviderWrapper;

class MailboxTextureBacking : public TextureBacking {
 public:
  explicit MailboxTextureBacking(
      sk_sp<SkImage> sk_image,
      const SkImageInfo& info,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper);
  explicit MailboxTextureBacking(
      const gpu::Mailbox& mailbox,
      const SkImageInfo& info,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper);
  const SkImageInfo& GetSkImageInfo() override;
  gpu::Mailbox GetMailbox() const override;
  sk_sp<SkImage> GetAcceleratedSkImage() override;
  sk_sp<SkImage> GetSkImageViaReadback() override;
  bool readPixels(const SkImageInfo& dst_info,
                  void* dst_pixels,
                  size_t dst_row_bytes,
                  int src_x,
                  int src_y) override;
  void FlushPendingSkiaOps() override;

 private:
  const sk_sp<SkImage> sk_image_;
  const gpu::Mailbox mailbox_;
  const SkImageInfo sk_image_info_;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_BACKING_H__
