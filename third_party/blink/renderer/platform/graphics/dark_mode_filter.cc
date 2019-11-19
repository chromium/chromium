// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_filter.h"

#include <cmath>

#include "base/logging.h"
#include "base/optional.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_color_filter.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_generic_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_icon_classifier.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_image_classifier.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
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

bool ShouldApplyToImage(const DarkModeSettings& settings,
                        const FloatRect& src_rect,
                        const FloatRect& dest_rect,
                        Image* image) {
  switch (settings.image_policy) {
    case DarkModeImagePolicy::kFilterSmart: {
      DarkModeImageClassifier* classifier;
      switch (settings.classifier_type) {
        case DarkModeClassifierType::kIcon: {
          DarkModeIconClassifier icon_classifier;
          classifier = &icon_classifier;
          break;
        }
        case DarkModeClassifierType::kGeneric: {
          DarkModeGenericClassifier generic_classifier;
          classifier = &generic_classifier;
          break;
        }
      }
      DarkModeClassification result =
          classifier->Classify(image, src_rect, dest_rect);
      return result == DarkModeClassification::kApplyFilter;
    }
    case DarkModeImagePolicy::kFilterNone:
      return false;
    case DarkModeImagePolicy::kFilterAll:
      return true;
  }
}

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

DarkModeFilter::DarkModeFilter()
    : text_classifier_(nullptr),
      color_filter_(nullptr),
      image_filter_(nullptr) {
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
}

Color DarkModeFilter::InvertColorIfNeeded(const Color& color,
                                          ElementRole role) {
  if (IsDarkModeActive() && ShouldApplyToColor(color, role))
    return color_filter_->InvertColor(color);
  return color;
}

void DarkModeFilter::ApplyToImageFlagsIfNeeded(const FloatRect& src_rect,
                                               const FloatRect& dest_rect,
                                               Image* image,
                                               cc::PaintFlags* flags) {
  if (!image_filter_ ||
      !ShouldApplyToImage(settings(), src_rect, dest_rect, image))
    return;
  flags->setColorFilter(image_filter_);
}

base::Optional<cc::PaintFlags> DarkModeFilter::ApplyToFlagsIfNeeded(
    const cc::PaintFlags& flags,
    ElementRole role) {
  if (!IsDarkModeActive())
    return base::nullopt;

  cc::PaintFlags dark_mode_flags = flags;
  if (flags.HasShader()) {
    dark_mode_flags.setColorFilter(color_filter_->ToSkColorFilter());
  } else if (ShouldApplyToColor(flags.getColor(), role)) {
    Color inverted_color = color_filter_->InvertColor(flags.getColor());
    dark_mode_flags.setColor(
        SkColorSetARGB(inverted_color.Alpha(), inverted_color.Red(),
                       inverted_color.Green(), inverted_color.Blue()));
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
bool DarkModeFilter::ShouldApplyToColor(const Color& color, ElementRole role) {
  switch (role) {
    case ElementRole::kText:
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
  }
  NOTREACHED();
}

}  // namespace blink
