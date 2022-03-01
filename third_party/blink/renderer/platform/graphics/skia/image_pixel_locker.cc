// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/skia/image_pixel_locker.h"

#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace blink {

namespace {

bool InfoIsCompatible(const SkImageInfo& info,
                      SkAlphaType alpha_type,
                      SkColorType color_type) {
  DCHECK_NE(alpha_type, kUnknown_SkAlphaType);

  if (info.colorType() != color_type)
    return false;

  // kOpaque_SkAlphaType works regardless of the requested alphaType.
  return info.alphaType() == alpha_type ||
         info.alphaType() == kOpaque_SkAlphaType;
}

}  // anonymous namespace

ImagePixelLocker::ImagePixelLocker(sk_sp<const SkImage> image,
                                   SkAlphaType alpha_type,
                                   SkColorType color_type)
    : image_(std::move(image)) {
  // If the image has in-RAM pixels and their format matches, use them directly.
  // TODO(fmalita): All current clients expect packed pixel rows.  Maybe we
  // could update them to support arbitrary rowBytes, and relax the check below.
  SkPixmap pixmap;
  image_->peekPixels(&pixmap);
  pixels_ = pixmap.addr();
  if (pixels_ && InfoIsCompatible(pixmap.info(), alpha_type, color_type) &&
      pixmap.rowBytes() == pixmap.info().minRowBytes()) {
    return;
  }

  pixels_ = nullptr;

  // No luck, we need to read the pixels into our local buffer.
  SkImageInfo info = SkImageInfo::Make(image_->width(), image_->height(),
                                       color_type, alpha_type);
  size_t row_bytes = info.minRowBytes();
  size_t size = info.computeByteSize(row_bytes);
  if (0 == size)
    return;

  // this will throw on failure
  pixel_storage_.resize(base::checked_cast<wtf_size_t>(size));
  pixmap.reset(info, pixel_storage_.data(), row_bytes);

  if (!image_->readPixels(pixmap, 0, 0))
    return;

  pixels_ = pixel_storage_.data();
}

}  // namespace blink
