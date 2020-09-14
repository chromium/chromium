// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include <cmath>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/lru_cache.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"

namespace blink {
namespace {

#if DCHECK_IS_ON()

// Floats that differ by this amount or less are considered to be equal.
const float kFloatEqualityEpsilon = 0.0001;

bool AreFloatsEqual(float a, float b) {
  return std::fabs(a - b) <= kFloatEqualityEpsilon;
}

void VerifySettingsAreUnchanged(const DarkModeSettings& a,
                                const DarkModeSettings& b) {
  if (a.mode == DarkModeInversionAlgorithm::kOff)
    return;

  DCHECK_EQ(a.image_policy, b.image_policy);
  DCHECK_EQ(a.text_brightness_threshold, b.text_brightness_threshold);
  DCHECK_EQ(a.grayscale, b.grayscale);
  DCHECK(AreFloatsEqual(a.contrast, b.contrast));
  DCHECK(AreFloatsEqual(a.image_grayscale_percent, b.image_grayscale_percent));
}

#endif  // DCHECK_IS_ON()

const size_t kMaxCacheSize = 1024u;
const int kMinImageLength = 8;
const int kMaxImageLength = 100;

// TODO(gilmanmh): If grayscaling images in dark mode proves popular among
// users, consider experimenting with different grayscale algorithms.
sk_sp<SkColorFilter> MakeGrayscaleFilter(float grayscale_percent) {
  DCHECK_GE(grayscale_percent, 0.0f);
  DCHECK_LE(grayscale_percent, 1.0f);

  SkColorMatrix grayscale_matrix;
  grayscale_matrix.setSaturation(1.0f - grayscale_percent);
  return SkColorFilters::Matrix(grayscale_matrix);
}

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

DarkModeFilter::DarkModeFilter()
    : text_classifier_(nullptr),
      background_classifier_(nullptr),
      image_classifier_(nullptr),
      color_filter_(nullptr),
      image_filter_(nullptr),
      inverted_color_cache_(new DarkModeInvertedColorCache()) {
  DarkModeSettings default_settings;
  default_settings.mode = DarkModeInversionAlgorithm::kOff;
  UpdateSettings(default_settings);
}

DarkModeFilter::~DarkModeFilter() {}

void DarkModeFilter::UpdateSettings(const DarkModeSettings& new_settings) {
  // Dark mode can be activated or deactivated on a per-page basis, depending on
  // whether the original page theme is already dark. However, there is
  // currently no mechanism to change the other settings after starting Chrome.
  // As such, if the mode doesn't change, we don't need to do anything.
  if (settings_.mode == new_settings.mode) {
#if DCHECK_IS_ON()
    VerifySettingsAreUnchanged(settings_, new_settings);
#endif
    return;
  }

  inverted_color_cache_->Clear();

  settings_ = new_settings;
  color_filter_ = DarkModeColorFilter::FromSettings(settings_);
  if (!color_filter_) {
    image_filter_ = nullptr;
    return;
  }

  if (settings_.image_grayscale_percent > 0.0f)
    image_filter_ = MakeGrayscaleFilter(settings_.image_grayscale_percent);
  else
    image_filter_ = color_filter_->ToSkColorFilter();

  text_classifier_ =
      DarkModeColorClassifier::MakeTextColorClassifier(settings_);
  background_classifier_ =
      DarkModeColorClassifier::MakeBackgroundColorClassifier(settings_);
  image_classifier_ = std::make_unique<DarkModeImageClassifier>();
}

SkColor DarkModeFilter::InvertColorIfNeeded(SkColor color, ElementRole role) {
  if (!IsDarkModeActive())
    return color;

  if (role_override_.has_value())
    role = role_override_.value();

  if (ShouldApplyToColor(color, role)) {
    return inverted_color_cache_->GetInvertedColor(color_filter_.get(), color);
  }

  return color;
}

bool DarkModeFilter::AnalyzeShouldApplyToImage(const SkRect& src,
                                               const SkRect& dst) {
  if (settings().image_policy == DarkModeImagePolicy::kFilterNone)
    return false;

  if (settings().image_policy == DarkModeImagePolicy::kFilterAll)
    return true;

  // Images being drawn from very smaller |src| rect, i.e. one of the dimensions
  // is very small, can be used for the border around the content or showing
  // separator. Consider these images irrespective of size of the rect being
  // drawn to. Classifying them will not be too costly.
  if (src.width() <= kMinImageLength || src.height() <= kMinImageLength)
    return true;

  // Do not consider images being drawn into bigger rect as these images are not
  // meant for icons or representing smaller widgets. These images are
  // considered as photos which should be untouched.
  return (dst.width() <= kMaxImageLength && dst.height() <= kMaxImageLength);
}

void DarkModeFilter::ApplyToImageFlagsIfNeeded(const SkRect& src,
                                               const SkRect& dst,
                                               const PaintImage& paint_image,
                                               cc::PaintFlags* flags) {
  // The construction of |paint_image| is expensive, so ensure
  // IsDarkModeActive() is checked prior to calling this function.
  // See: https://crbug.com/1094781.
  DCHECK(IsDarkModeActive());

  if (!image_filter_ || !AnalyzeShouldApplyToImage(src, dst))
    return;

  if (ClassifyImage(settings(), src, dst, paint_image) ==
      DarkModeClassification::kApplyFilter)
    flags->setColorFilter(image_filter_);
}

base::Optional<cc::PaintFlags> DarkModeFilter::ApplyToFlagsIfNeeded(
    const cc::PaintFlags& flags,
    ElementRole role) {
  if (!IsDarkModeActive())
    return base::nullopt;

  if (role_override_.has_value())
    role = role_override_.value();

  cc::PaintFlags dark_mode_flags = flags;
  if (flags.HasShader()) {
    dark_mode_flags.setColorFilter(color_filter_->ToSkColorFilter());
  } else if (ShouldApplyToColor(flags.getColor(), role)) {
    dark_mode_flags.setColor(inverted_color_cache_->GetInvertedColor(
        color_filter_.get(), flags.getColor()));
  }

  return base::make_optional<cc::PaintFlags>(std::move(dark_mode_flags));
}

bool DarkModeFilter::IsDarkModeActive() const {
  return !!color_filter_;
}

// We don't check IsDarkModeActive() because the caller is expected to have
// already done so. This allows the caller to exit earlier if it needs to
// perform some other logic in between confirming dark mode is active and
// checking the color classifiers.
bool DarkModeFilter::ShouldApplyToColor(SkColor color, ElementRole role) {
  switch (role) {
    case ElementRole::kText:
      DCHECK(text_classifier_);
      return text_classifier_->ShouldInvertColor(color) ==
             DarkModeClassification::kApplyFilter;
    case ElementRole::kListSymbol:
      // TODO(prashant.n): Rename text_classifier_ to foreground_classifier_,
      // so that same classifier can be used for all roles which are supposed
      // to be at foreground.
      DCHECK(text_classifier_);
      return text_classifier_->ShouldInvertColor(color) ==
             DarkModeClassification::kApplyFilter;
    case ElementRole::kBackground:
      DCHECK(background_classifier_);
      return background_classifier_->ShouldInvertColor(color) ==
             DarkModeClassification::kApplyFilter;
    case ElementRole::kSVG:
      // 1) Inline SVG images are considered as individual shapes and do not
      // have an Image object associated with them. So they do not go through
      // the regular image classification pipeline. Do not apply any filter to
      // the SVG shapes until there is a way to get the classification for the
      // entire image to which these shapes belong.

      // 2) Non-inline SVG images are already classified at this point and have
      // a filter applied if necessary.
      return false;
    default:
      return false;
  }
  NOTREACHED();
}

size_t DarkModeFilter::GetInvertedColorCacheSizeForTesting() {
  return inverted_color_cache_->size();
}

DarkModeClassification DarkModeFilter::ClassifyImage(
    const DarkModeSettings& settings,
    const SkRect& src,
    const SkRect& dst,
    const PaintImage& paint_image) {
  switch (settings.image_policy) {
    case DarkModeImagePolicy::kFilterSmart:
      return image_classifier_->Classify(paint_image, src, dst);
    case DarkModeImagePolicy::kFilterNone:
      return DarkModeClassification::kDoNotApplyFilter;
    case DarkModeImagePolicy::kFilterAll:
      return DarkModeClassification::kApplyFilter;
  }

  NOTREACHED();
}

ScopedDarkModeElementRoleOverride::ScopedDarkModeElementRoleOverride(
    GraphicsContext* graphics_context,
    DarkModeFilter::ElementRole role)
    : graphics_context_(graphics_context) {
  DarkModeFilter& dark_mode_filter = graphics_context->dark_mode_filter_;
  previous_role_override_ = dark_mode_filter.role_override_;
  dark_mode_filter.role_override_ = role;
}

ScopedDarkModeElementRoleOverride::~ScopedDarkModeElementRoleOverride() {
  DarkModeFilter& dark_mode_filter = graphics_context_->dark_mode_filter_;
  dark_mode_filter.role_override_ = previous_role_override_;
}

}  // namespace blink
