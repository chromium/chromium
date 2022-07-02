// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include <cmath>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/lru_cache.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_cache.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/skia/include/core/SkColorFilter.h"

namespace blink {

namespace {

const size_t kMaxCacheSize = 1024u;

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
    DarkModeFilter* filter,
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
    color_filter = filter->GenerateImageFilter(pixmap, rounded_src);

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

// DarkModeInvertedColorCache - Implements cache for inverted colors.
class DarkModeInvertedColorCache {
 public:
  DarkModeInvertedColorCache() : cache_(kMaxCacheSize) {}
  ~DarkModeInvertedColorCache() = default;

  SkColor GetInvertedColor(DarkModeColorFilter* filter, SkColor color) {
    SkColor key(color);
    auto it = cache_.Get(key);
    if (it != cache_.end())
      return it->second;

    SkColor inverted_color = filter->InvertColor(color);
    cache_.Put(key, static_cast<SkColor>(inverted_color));
    return inverted_color;
  }

  void Clear() { cache_.Clear(); }

  size_t size() { return cache_.size(); }

 private:
  base::HashingLRUCache<SkColor, SkColor> cache_;
};

DarkModeFilter::DarkModeFilter(const DarkModeSettings& settings)
    : immutable_(settings),
      inverted_color_cache_(new DarkModeInvertedColorCache()) {}

DarkModeFilter::~DarkModeFilter() {}

DarkModeFilter::ImmutableData::ImmutableData(const DarkModeSettings& settings)
    : settings(settings),
      foreground_classifier(nullptr),
      background_classifier(nullptr),
      image_classifier(nullptr),
      color_filter(nullptr),
      image_filter(nullptr) {
  color_filter = DarkModeColorFilter::FromSettings(settings);
  if (!color_filter)
    return;

  image_filter = color_filter->ToSkColorFilter();

  foreground_classifier =
      DarkModeColorClassifier::MakeForegroundColorClassifier(settings);
  background_classifier =
      DarkModeColorClassifier::MakeBackgroundColorClassifier(settings);
  image_classifier = std::make_unique<DarkModeImageClassifier>();
}

DarkModeImagePolicy DarkModeFilter::GetDarkModeImagePolicy() const {
  return immutable_.settings.image_policy;
}

SkColor DarkModeFilter::InvertColorIfNeeded(SkColor color, ElementRole role) {
  if (!immutable_.color_filter)
    return color;

  if (ShouldApplyToColor(color, role)) {
    return inverted_color_cache_->GetInvertedColor(
        immutable_.color_filter.get(), color);
  }

  return color;
}

void DarkModeFilter::ApplyFilterToImage(Image* image,
                                        cc::PaintFlags* flags,
                                        const SkRect& src) {
  DCHECK(image);
  DCHECK(flags);
  DCHECK_NE(GetDarkModeImagePolicy(), DarkModeImagePolicy::kFilterNone);

  if (GetDarkModeImagePolicy() == DarkModeImagePolicy::kFilterAll) {
    flags->setColorFilter(GetImageFilter());
    return;
  }

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
      GetDarkModeFilterForImageOnMainThread(this, image, src.roundOut());
  if (color_filter)
    flags->setColorFilter(std::move(color_filter));
}

bool DarkModeFilter::ShouldApplyFilterToImage(ImageType type) const {
  DarkModeImagePolicy image_policy = GetDarkModeImagePolicy();
  if (image_policy == DarkModeImagePolicy::kFilterNone)
    return false;
  if (image_policy == DarkModeImagePolicy::kFilterAll)
    return true;

  // kIcon: Do not consider images being drawn into bigger rect as these
  // images are not meant for icons or representing smaller widgets. These
  // images are considered as photos which should be untouched.
  // kSeparator: Images being drawn from very smaller |src| rect, i.e. one of
  // the dimensions is very small, can be used for the border around the content
  // or showing separator. Consider these images irrespective of size of the
  // rect being drawn to. Classifying them will not be too costly.
  return type == ImageType::kIcon || type == ImageType::kSeparator;
}

sk_sp<SkColorFilter> DarkModeFilter::GenerateImageFilter(
    const SkPixmap& pixmap,
    const SkIRect& src) const {
  DCHECK(immutable_.settings.image_policy == DarkModeImagePolicy::kFilterSmart);
  DCHECK(immutable_.image_filter);

  return (immutable_.image_classifier->Classify(pixmap, src) ==
          DarkModeResult::kApplyFilter)
             ? immutable_.image_filter
             : nullptr;
}

sk_sp<SkColorFilter> DarkModeFilter::GetImageFilter() const {
  DCHECK(immutable_.image_filter);
  return immutable_.image_filter;
}

DarkModeFilter::ElementRole DarkModeFilter::BorderElementRole(
    SkColor border_color,
    SkColor background_color) {
  if (background_color == 0 ||
      DarkModeColorClassifier::CalculateColorBrightness(border_color) <
          DarkModeColorClassifier::CalculateColorBrightness(background_color))
    return ElementRole::kForeground;
  return ElementRole::kBackground;
}

absl::optional<cc::PaintFlags> DarkModeFilter::ApplyToFlagsIfNeeded(
    const cc::PaintFlags& flags,
    ElementRole role) {
  if (!immutable_.color_filter || flags.HasShader() ||
      !ShouldApplyToColor(flags.getColor(), role))
    return absl::nullopt;

  cc::PaintFlags dark_mode_flags = flags;
  dark_mode_flags.setColor(inverted_color_cache_->GetInvertedColor(
      immutable_.color_filter.get(), flags.getColor()));

  return absl::make_optional<cc::PaintFlags>(std::move(dark_mode_flags));
}

bool DarkModeFilter::ShouldApplyToColor(SkColor color, ElementRole role) {
  switch (role) {
    case ElementRole::kSVG:
    case ElementRole::kForeground:
    case ElementRole::kListSymbol:
      DCHECK(immutable_.foreground_classifier);
      return immutable_.foreground_classifier->ShouldInvertColor(color) ==
             DarkModeResult::kApplyFilter;
    case ElementRole::kBackground:
      DCHECK(immutable_.background_classifier);
      return immutable_.background_classifier->ShouldInvertColor(color) ==
             DarkModeResult::kApplyFilter;
    default:
      return false;
  }
  NOTREACHED();
}

size_t DarkModeFilter::GetInvertedColorCacheSizeForTesting() {
  return inverted_color_cache_->size();
}

}  // namespace blink
