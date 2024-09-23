// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/mailbox_texture_backing.h"

#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/mailbox_ref.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"

namespace blink {

MailboxTextureBacking::MailboxTextureBacking(
    sk_sp<SkImage> sk_image,
    scoped_refptr<MailboxRef> mailbox_ref,
    const SkImageInfo& info,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : sk_image_(std::move(sk_image)),
      mailbox_ref_(std::move(mailbox_ref)),
      sk_image_info_(info),
      context_provider_wrapper_(std::move(context_provider_wrapper)) {}

MailboxTextureBacking::MailboxTextureBacking(
    const gpu::Mailbox& mailbox,
    scoped_refptr<MailboxRef> mailbox_ref,
    const SkImageInfo& info,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : mailbox_(mailbox),
      mailbox_ref_(std::move(mailbox_ref)),
      sk_image_info_(info),
      context_provider_wrapper_(std::move(context_provider_wrapper)) {}

MailboxTextureBacking::~MailboxTextureBacking() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (context_provider_wrapper_) {
    gpu::raster::RasterInterface* ri =
        context_provider_wrapper_->ContextProvider()->RasterInterface();
    // Update the sync token for MailboxRef.
    ri->WaitSyncTokenCHROMIUM(mailbox_ref_->sync_token().GetConstData());
    gpu::SyncToken sync_token;
    ri->GenUnverifiedSyncTokenCHROMIUM(sync_token.GetData());
    mailbox_ref_->set_sync_token(sync_token);
  }
}

const SkImageInfo& MailboxTextureBacking::GetSkImageInfo() {
  return sk_image_info_;
}

gpu::Mailbox MailboxTextureBacking::GetMailbox() const {
  return mailbox_;
}

sk_sp<SkImage> MailboxTextureBacking::GetAcceleratedSkImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return sk_image_;
}

sk_sp<SkImage> MailboxTextureBacking::GetSkImageViaReadback() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!mailbox_.IsZero()) {
    if (!context_provider_wrapper_)
      return nullptr;
    // TODO(jochin): Consider doing some caching and using discardable memory.
    sk_sp<SkData> image_pixels =
        TryAllocateSkData(sk_image_info_.computeMinByteSize());
    if (!image_pixels)
      return nullptr;
    uint8_t* writable_pixels =
        static_cast<uint8_t*>(image_pixels->writable_data());
    gpu::raster::RasterInterface* ri =
        context_provider_wrapper_->ContextProvider()->RasterInterface();
    if (!ri->ReadbackImagePixels(
            mailbox_, sk_image_info_,
            static_cast<GLuint>(sk_image_info_.minRowBytes()), 0, 0,
            /*plane_index=*/0, writable_pixels)) {
      return nullptr;
    }

    return SkImages::RasterFromData(sk_image_info_, std::move(image_pixels),
                                    sk_image_info_.minRowBytes());
  } else if (sk_image_) {
    return sk_image_->makeNonTextureImage();
  }
  return nullptr;
}

bool MailboxTextureBacking::readPixels(const SkImageInfo& dst_info,
                                       void* dst_pixels,
                                       size_t dst_row_bytes,
                                       int src_x,
                                       int src_y) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!mailbox_.IsZero()) {
    if (!context_provider_wrapper_)
      return false;

    gpu::raster::RasterInterface* ri =
        context_provider_wrapper_->ContextProvider()->RasterInterface();
    return ri->ReadbackImagePixels(mailbox_, dst_info,
                                   static_cast<GLuint>(dst_info.minRowBytes()),
                                   src_x, src_y, /*plane_index=*/0, dst_pixels);
  } else if (sk_image_) {
    return sk_image_->readPixels(dst_info, dst_pixels, dst_row_bytes, src_x,
                                 src_y);
  }
  return false;
}

void MailboxTextureBacking::FlushPendingSkiaOps() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!context_provider_wrapper_ || !sk_image_) {
    return;
  }
  GrDirectContext* ctx =
      context_provider_wrapper_->ContextProvider()->GetGrContext();
  if (!ctx) {
    return;
  }
  ctx->flushAndSubmit(sk_image_);
}

}  // namespace blink
