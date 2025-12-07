// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/mailbox_texture_backing.h"

#include "components/viz/common/gpu/raster_context_provider.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_ref.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

MailboxTextureBacking::MailboxTextureBacking(
    scoped_refptr<gpu::ClientSharedImage> shared_image,
    scoped_refptr<MailboxRef> mailbox_ref,
    const gfx::Size& size,
    const viz::SharedImageFormat& format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space,
    scoped_refptr<viz::RasterContextProvider> context_provider)
    : shared_image_(std::move(shared_image)),
      mailbox_ref_(std::move(mailbox_ref)),
      sk_image_info_(SkImageInfo::Make(gfx::SizeToSkISize(size),
                                       ToClosestSkColorType(format),
                                       alpha_type,
                                       color_space.ToSkColorSpace())),
      context_provider_(std::move(context_provider)) {
  CHECK(context_provider_);
  gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
  scoped_access_ = shared_image_->BeginRasterAccess(
      ri, mailbox_ref_->sync_token(), /*readonly=*/true);
}

MailboxTextureBacking::~MailboxTextureBacking() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
  // Update the sync token for MailboxRef.
  ri->WaitSyncTokenCHROMIUM(mailbox_ref_->sync_token().GetConstData());
  gpu::SyncToken sync_token =
      gpu::RasterScopedAccess::EndAccess(std::move(scoped_access_));
  mailbox_ref_->set_sync_token(sync_token);
}

const SkImageInfo& MailboxTextureBacking::GetSkImageInfo() {
  return sk_image_info_;
}

gpu::Mailbox MailboxTextureBacking::GetMailbox() const {
  return shared_image_->mailbox();
}

sk_sp<SkImage> MailboxTextureBacking::GetSkImageViaReadback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(jochin): Consider doing some caching and using discardable memory.
  sk_sp<SkData> image_pixels =
      TryAllocateSkData(sk_image_info_.computeMinByteSize());
  if (!image_pixels) {
    return nullptr;
  }
  uint8_t* writable_pixels =
      static_cast<uint8_t*>(image_pixels->writable_data());
  gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
  if (!ri->ReadbackImagePixels(
          GetMailbox(), sk_image_info_,
          static_cast<GLuint>(sk_image_info_.minRowBytes()), 0, 0,
          /*plane_index=*/0, writable_pixels)) {
    return nullptr;
  }

  return SkImages::RasterFromData(sk_image_info_, std::move(image_pixels),
                                  sk_image_info_.minRowBytes());
}

bool MailboxTextureBacking::readPixels(const SkImageInfo& dst_info,
                                       void* dst_pixels,
                                       size_t dst_row_bytes,
                                       int src_x,
                                       int src_y) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  gpu::raster::RasterInterface* ri = context_provider_->RasterInterface();
  return ri->ReadbackImagePixels(GetMailbox(), dst_info,
                                 static_cast<GLuint>(dst_info.minRowBytes()),
                                 src_x, src_y, /*plane_index=*/0, dst_pixels);
}

}  // namespace blink
