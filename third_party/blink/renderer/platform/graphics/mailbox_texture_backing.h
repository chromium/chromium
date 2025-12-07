// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_BACKING_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/mailbox.h"

namespace gpu {
class ClientSharedImage;
class RasterScopedAccess;
}  // namespace gpu

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace blink {
class MailboxRef;

class MailboxTextureBacking : public TextureBacking {
 public:
  explicit MailboxTextureBacking(
      scoped_refptr<gpu::ClientSharedImage> shared_image,
      scoped_refptr<MailboxRef> mailbox_ref,
      const gfx::Size& size,
      const viz::SharedImageFormat& format,
      SkAlphaType alpha_type,
      const gfx::ColorSpace& color_space,
      scoped_refptr<viz::RasterContextProvider> context_provider);
  ~MailboxTextureBacking() override;
  const SkImageInfo& GetSkImageInfo() override;
  gpu::Mailbox GetMailbox() const override;
  sk_sp<SkImage> GetSkImageViaReadback() override;
  bool readPixels(const SkImageInfo& dst_info,
                  void* dst_pixels,
                  size_t dst_row_bytes,
                  int src_x,
                  int src_y) override;

 private:
  scoped_refptr<gpu::ClientSharedImage> shared_image_;
  scoped_refptr<MailboxRef> mailbox_ref_;
  std::unique_ptr<gpu::RasterScopedAccess> scoped_access_;
  const SkImageInfo sk_image_info_;
  scoped_refptr<viz::RasterContextProvider> context_provider_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_MAILBOX_TEXTURE_BACKING_H_
