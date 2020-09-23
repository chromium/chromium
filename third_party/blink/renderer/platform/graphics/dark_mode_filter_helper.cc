// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter_helper.h"

#include "base/hash/hash.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_cache.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

// static
void DarkModeFilterHelper::ApplyToImageIfNeeded(
    DarkModeFilter* dark_mode_filter,
    Image* image,
    cc::PaintFlags* flags,
    const SkRect& src,
    const SkRect& dst) {
  DCHECK(dark_mode_filter);
  DCHECK(image);
  DCHECK(flags);

  // The Image::AsSkBitmapForCurrentFrame() is expensive due creation of paint
  // image and bitmap, so ensure IsDarkModeActive() is checked prior to calling
  // this function. See: https://crbug.com/1094781.
  DCHECK(dark_mode_filter->IsDarkModeActive());

  sk_sp<SkColorFilter> filter;
  DarkModeResult result = dark_mode_filter->AnalyzeShouldApplyToImage(src, dst);

  if (result == DarkModeResult::kApplyFilter) {
    filter = dark_mode_filter->GetImageFilter();
  } else if (result == DarkModeResult::kNotClassified) {
    DarkModeImageCache* cache = image->GetDarkModeImageCache();
    DCHECK(cache);
    if (cache->Exists(src)) {
      filter = cache->Get(src);
    } else {
      // Performance warning: Calling this function will synchronously decode
      // image.
      SkBitmap bitmap =
          image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
      SkPixmap pixmap;
      bitmap.peekPixels(&pixmap);
      filter = dark_mode_filter->ApplyToImage(pixmap, src, dst);
      cache->Add(src, filter);
    }
  }

  if (filter)
    flags->setColorFilter(filter);
}

}  // namespace blink
