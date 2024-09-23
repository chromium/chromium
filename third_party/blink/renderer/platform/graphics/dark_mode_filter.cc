// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include <cmath>
#include <optional>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/containers/lru_cache.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_cache.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_utils.h"

namespace blink {

namespace {

const size_t kMaxCacheSize = 1024u;
constexpr SkColor SK_ColorDark = SkColorSetARGB(0xFF, 0x12, 0x12, 0x12);

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

sk_sp<cc::ColorFilter> GetDarkModeFilterForImageOnMainThread(
    DarkModeFilter* filter,
    Image* image,
    const SkIRect& rounded_src) {
  sk_sp<cc::ColorFilter> color_filter;
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

  SkColor4f GetInvertedColor(DarkModeColorFilter* filter, SkColor4f color) {
    SkColor key = color.toSkColor();
    auto it = cache_.Get(key);
    if (it != cache_.end())
      return it->second;

    SkColor4f inverted_color = filter->InvertColor(color);
    cache_.Put(key, inverted_color);
    return inverted_color;
  }

  void Clear() { cache_.Clear(); }

  size_t size() { return cache_.size(); }

 private:
  base::HashingLRUCache<SkColor, SkColor4f> cache_;
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

  image_filter = color_filter->ToColorFilter();

  foreground_classifier =
      DarkModeColorClassifier::MakeForegroundColorClassifier(settings);
  background_classifier =
      DarkModeColorClassifier::MakeBackgroundColorClassifier(settings);
  image_classifier = std::make_unique<DarkModeImageClassifier>(
      settings.image_classifier_policy);
}

DarkModeImagePolicy DarkModeFilter::GetDarkModeImagePolicy() const {
  return immutable_.settings.image_policy;
}

// Heuristic to maintain contrast for borders and selections (see:
// crbug.com/1263545,crbug.com/1298969)
SkColor4f DarkModeFilter::AdjustDarkenColor(
    const SkColor4f& color,
    DarkModeFilter::ElementRole role,
    const SkColor4f& contrast_background) {
  const SkColor4f& background = [&contrast_background]() {
    if (contrast_background == SkColors::kTransparent)
      return SkColor4f::FromColor(SK_ColorDark);
    else
      return contrast_background;
  }();

  switch (role) {
    case ElementRole::kBorder: {
      if (color == SkColor4f{0.0f, 0.0f, 0.0f, color.fA})
        return color;

      if (color_utils::GetContrastRatio(color, background) <
          color_utils::kMinimumReadableContrastRatio)
        return color;

      return AdjustDarkenColor(Color::FromSkColor4f(color).Dark().toSkColor4f(),
                               role, background);
    }
    case ElementRole::kSelection: {
      if (!immutable_.color_filter)
        return color;

      return immutable_.color_filter->AdjustColorForHigherConstrast(
          color, background, color_utils::kMinimumVisibleContrastRatio);
    }
    default:
      return color;
  }
  NOTREACHED_IN_MIGRATION();
}

SkColor4f DarkModeFilter::InvertColorIfNeeded(
    const SkColor4f& color,
    ElementRole role,
    const SkColor4f& contrast_background) {
  return AdjustDarkenColor(
      InvertColorIfNeeded(color, role), role,
      InvertColorIfNeeded(contrast_background, ElementRole::kBackground));
}

SkColor4f DarkModeFilter::InvertColorIfNeeded(const SkColor4f& color,
                                              ElementRole role) {
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
  sk_sp<cc::ColorFilter> color_filter =
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

sk_sp<cc::ColorFilter> DarkModeFilter::GenerateImageFilter(
    const SkPixmap& pixmap,
    const SkIRect& src) const {
  DCHECK(immutable_.settings.image_policy == DarkModeImagePolicy::kFilterSmart);
  DCHECK(immutable_.image_filter);

  return (immutable_.image_classifier->Classify(pixmap, src) ==
          DarkModeResult::kApplyFilter)
             ? immutable_.image_filter
             : nullptr;
}

sk_sp<cc::ColorFilter> DarkModeFilter::GetImageFilter() const {
  DCHECK(immutable_.image_filter);
  return immutable_.image_filter;
}

std::optional<cc::PaintFlags> DarkModeFilter::ApplyToFlagsIfNeeded(
    const cc::PaintFlags& flags,
    ElementRole role,
    SkColor4f contrast_background) {
  if (!immutable_.color_filter || flags.HasShader())
    return std::nullopt;

  cc::PaintFlags dark_mode_flags = flags;
  SkColor4f flags_color = flags.getColor4f();
  if (ShouldApplyToColor(flags_color, role)) {
    flags_color = inverted_color_cache_->GetInvertedColor(
        immutable_.color_filter.get(), flags_color);
  }
  dark_mode_flags.setColor(AdjustDarkenColor(
      flags_color, role,
      InvertColorIfNeeded(contrast_background, ElementRole::kBackground)));

  return std::make_optional<cc::PaintFlags>(std::move(dark_mode_flags));
}

bool DarkModeFilter::ShouldApplyToColor(const SkColor4f& color,
                                        ElementRole role) {
  switch (role) {
    case ElementRole::kBorder:
    case ElementRole::kSVG:
    case ElementRole::kForeground:
    case ElementRole::kListSymbol:
      DCHECK(immutable_.foreground_classifier);
      return immutable_.foreground_classifier->ShouldInvertColor(
                 color.toSkColor()) == DarkModeResult::kApplyFilter;
    case ElementRole::kBackground:
    case ElementRole::kSelection:
      DCHECK(immutable_.background_classifier);
      return immutable_.background_classifier->ShouldInvertColor(
                 color.toSkColor()) == DarkModeResult::kApplyFilter;
    default:
      return false;
  }
  NOTREACHED_IN_MIGRATION();
}

size_t DarkModeFilter::GetInvertedColorCacheSizeForTesting() {
  return inverted_color_cache_->size();
}

}  // namespace blink
