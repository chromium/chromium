// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/skia/image_pixel_locker.h"

#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace blink {

ImagePixelLocker::ImagePixelLocker(sk_sp<const SkImage> image)
    : image_(std::move(image)) {
  // If the image has in-RAM pixels and their format matches, use them directly.
  // TODO(fmalita): All current clients expect packed pixel rows.  Maybe we
  // could update them to support arbitrary rowBytes, and relax the check below.
  image_->peekPixels(&pixmap_);
  if (pixmap_.addr() && pixmap_.rowBytes() == pixmap_.info().minRowBytes()) {
    pixmap_valid_ = true;
    return;
  }

  // No luck, we need to read the pixels into our local buffer.
  SkImageInfo info =
      SkImageInfo::Make(image_->width(), image_->height(), image_->colorType(),
                        image_->alphaType());
  size_t row_bytes = info.minRowBytes();
  size_t size = info.computeByteSize(row_bytes);
  if (0 == size)
    return;

  // this will throw on failure
  pixel_storage_.resize(base::checked_cast<wtf_size_t>(size));
  pixmap_.reset(info, pixel_storage_.data(), row_bytes);

  if (!image_->readPixels(pixmap_, 0, 0))
    return;

  pixmap_valid_ = true;
}

}  // namespace blink
