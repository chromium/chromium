// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_IMAGE_PIXEL_LOCKER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_IMAGE_PIXEL_LOCKER_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImage;

namespace blink {

class ImagePixelLocker final {
  DISALLOW_NEW();

 public:
  ImagePixelLocker(sk_sp<const SkImage>, SkAlphaType, SkColorType);
  ImagePixelLocker(const ImagePixelLocker&) = delete;
  ImagePixelLocker& operator=(const ImagePixelLocker&) = delete;

  const SkPixmap* GetSkPixmap() const {
    return pixmap_valid_ ? &pixmap_ : nullptr;
  }

 private:
  const sk_sp<const SkImage> image_;
  Vector<char> pixel_storage_;
  // `pixmap_` will either point to `image_`'s storage, or `pixel_storage_`.
  bool pixmap_valid_ = false;
  SkPixmap pixmap_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_SKIA_IMAGE_PIXEL_LOCKER_H_
