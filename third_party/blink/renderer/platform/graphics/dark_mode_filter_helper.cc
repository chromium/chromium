// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter_helper.h"

#include "base/command_line.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_cache.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

namespace blink {

namespace {

bool IsRasterSideDarkModeForImagesEnabled() {
  static bool enabled = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableRasterSideDarkModeForImages);
  return enabled;
}

bool ShouldUseRasterSidePath(Image* image) {
  DCHECK(image);

  // Raster-side path is not enabled.
  if (!IsRasterSideDarkModeForImagesEnabled())
    return false;

  // Raster-side path is only supported for bitmap images.
  return image->IsBitmapImage();
}

sk_sp<SkColorFilter> GetDarkModeFilterForImageOnMainThread(
    DarkModeFilter& filter,
    Image* image,
    const SkIRect& rounded_src) {
  SCOPED_BLINK_UMA_HISTOGRAM_TIMER("Blink.DarkMode.ApplyToImageOnMainThread");

  sk_sp<SkColorFilter> color_filter;
  DarkModeImageCache* cache = image->GetDarkModeImageCache();
  DCHECK(cache);
  if (cache->Exists(rounded_src)) {
    color_filter = cache->Get(rounded_src);
  } else {
    // Performance warning: Calling AsSkBitmapForCurrentFrame() will
    // synchronously decode image.
    SkBitmap bitmap =
        image->AsSkBitmapForCurrentFrame(kDoNotRespectImageOrientation);
    SkPixmap pixmap;
    bitmap.peekPixels(&pixmap);
    color_filter = filter.ApplyToImage(pixmap, rounded_src);

    // Using blink side dark mode for images, it is hard to implement
    // caching mechanism for partially loaded bitmap image content, as
    // content id for the image frame being rendered gets decided during
    // rastering only. So caching of dark mode result will be deferred until
    // default frame is completely received. This will help get correct
    // classification results for incremental content received for the given
    // image.
    if (!image->IsBitmapImage() || image->CurrentFrameIsComplete())
      cache->Add(rounded_src, color_filter);
  }
  return color_filter;
}

}  // namespace

// static
void DarkModeFilterHelper::ApplyToImageIfNeeded(DarkModeFilter& filter,
                                                Image* image,
                                                cc::PaintFlags* flags,
                                                const SkRect& src,
                                                const SkRect& dst) {
  DCHECK(image);
  DCHECK(flags);

  DarkModeImagePolicy image_policy = filter.GetDarkModeImagePolicy();
  if (image_policy == DarkModeImagePolicy::kFilterNone)
    return;

  if (image_policy == DarkModeImagePolicy::kFilterAll) {
    flags->setColorFilter(filter.GetImageFilter());
    return;
  }

  SkIRect rounded_src = src.roundOut();
  SkIRect rounded_dst = dst.roundOut();
  if (filter.ImageShouldHaveFilterAppliedBasedOnSizes(rounded_src,
                                                      rounded_dst)) {
    // Raster-side dark mode path - Just set the dark mode on flags and dark
    // mode will be applied at compositor side during rasterization.
    if (ShouldUseRasterSidePath(image)) {
      flags->setUseDarkModeForImage(true);
      return;
    }

    // Blink-side dark mode path - Apply dark mode to images in main thread
    // only. If the result is not cached, calling this path is expensive and
    // will block main thread.
    sk_sp<SkColorFilter> color_filter =
        GetDarkModeFilterForImageOnMainThread(filter, image, rounded_src);
    if (color_filter)
      flags->setColorFilter(std::move(color_filter));
  }
}

}  // namespace blink
