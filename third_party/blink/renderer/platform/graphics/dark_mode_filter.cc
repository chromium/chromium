// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include <cmath>

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/lru_cache.h"
#include "third_party/skia/include/core/SkColorFilter.h"

namespace blink {
namespace {

const size_t kMaxCacheSize = 1024u;
const int kMinImageLength = 8;
const int kMaxImageLength = 100;

}  // namespace

// DarkModeInvertedColorCache - Implements cache for inverted colors.
class DarkModeInvertedColorCache {
 public:
  DarkModeInvertedColorCache() : cache_(kMaxCacheSize) {}
  ~DarkModeInvertedColorCache() = default;

  SkColor GetInvertedColor(DarkModeColorFilter* filter, SkColor color) {
    WTF::IntegralWithAllKeys<SkColor> key(color);
    SkColor* cached_value = cache_.Get(key);
    if (cached_value)
      return *cached_value;

    SkColor inverted_color = filter->InvertColor(color);
    cache_.Put(key, static_cast<SkColor>(inverted_color));
    return inverted_color;
  }

  void Clear() { cache_.Clear(); }

  size_t size() { return cache_.size(); }

 private:
  WTF::LruCache<WTF::IntegralWithAllKeys<SkColor>, SkColor> cache_;
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

bool DarkModeFilter::ImageShouldHaveFilterAppliedBasedOnSizes(
    const SkIRect& src,
    const SkIRect& dst) const {
  // Images being drawn from very smaller |src| rect, i.e. one of the dimensions
  // is very small, can be used for the border around the content or showing
  // separator. Consider these images irrespective of size of the rect being
  // drawn to. Classifying them will not be too costly.
  if (src.width() <= kMinImageLength || src.height() <= kMinImageLength)
    return true;

  // Do not consider images being drawn into bigger rect as these images are not
  // meant for icons or representing smaller widgets. These images are
  // considered as photos which should be untouched.
  return dst.width() <= kMaxImageLength && dst.height() <= kMaxImageLength;
}

sk_sp<SkColorFilter> DarkModeFilter::ApplyToImage(const SkPixmap& pixmap,
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
