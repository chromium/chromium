// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter_helper.h"

#include "base/command_line.h"
#include "base/hash/hash.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_cache.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"

namespace blink {

namespace {
bool IsRasterSideDarkModeForImagesEnabled() {
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableRasterSideDarkModeForImages);
  return enabled;
}
}  // namespace

// static
SkColor DarkModeFilterHelper::ApplyToColorIfNeeded(
    GraphicsContext* context,
    SkColor color,
    DarkModeFilter::ElementRole role) {
  DCHECK(context);
  return context->IsDarkModeEnabled()
             ? context->GetDarkModeFilter()->InvertColorIfNeeded(color, role)
             : color;
}

// static
void DarkModeFilterHelper::ApplyToImageIfNeeded(GraphicsContext* context,
                                                Image* image,
                                                cc::PaintFlags* flags,
                                                const SkRect& src,
                                                const SkRect& dst) {
  DCHECK(context && context->GetDarkModeFilter());
  DCHECK(image);
  DCHECK(flags);

  // The Image::AsSkBitmapForCurrentFrame() is expensive due paint image and
  // bitmap creation, so return if dark mode is not enabled. For details see:
  // https://crbug.com/1094781.
  if (!context->IsDarkModeEnabled())
    return;

  // For RSDM, just set the dark mode on flags and rest will be taken care at
  // compositor side.
  if (image->IsBitmapImage() && IsRasterSideDarkModeForImagesEnabled()) {
    flags->setUseDarkModeForImage(true);
    return;
  }

  SkIRect rounded_src = src.roundOut();
  SkIRect rounded_dst = dst.roundOut();

  sk_sp<SkColorFilter> filter;
  DarkModeResult result =
      context->GetDarkModeFilter()->AnalyzeShouldApplyToImage(rounded_src,
                                                              rounded_dst);

  if (result == DarkModeResult::kApplyFilter) {
    filter = context->GetDarkModeFilter()->GetImageFilter();
  } else if (result == DarkModeResult::kNotClassified) {
    DarkModeImageCache* cache = image->GetDarkModeImageCache();
    DCHECK(cache);
    if (cache->Exists(rounded_src)) {
      filter = cache->Get(rounded_src);
    } else {
      // Performance warning: Calling this function will synchronously decode
      // image.
      SkBitmap bitmap =
          image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
      SkPixmap pixmap;
      bitmap.peekPixels(&pixmap);
      filter = context->GetDarkModeFilter()->ApplyToImage(pixmap, rounded_src,
                                                          rounded_dst);

      // Using blink side dark mode for images, it is hard to implement caching
      // mechanism for partially loaded bitmap image content, as content id for
      // the image frame being rendered gets decided during rastering only. So
      // caching of dark mode result will be deferred until default frame is
      // completely received. This will help get correct classification results
      // for incremental content received for the given image.
      if (!image->IsBitmapImage() || image->CurrentFrameIsComplete())
        cache->Add(rounded_src, filter);
    }
  }

  if (filter)
    flags->setColorFilter(filter);
}

}  // namespace blink
