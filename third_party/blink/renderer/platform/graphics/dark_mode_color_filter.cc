// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_color_filter.h"

#include "base/logging.h"
#include "third_party/blink/renderer/platform/graphics/lab_color_space.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/effects/SkTableColorFilter.h"

namespace blink {
namespace {

sk_sp<SkColorFilter> SkColorFilterFromSettings(
    SkHighContrastConfig::InvertStyle invert_style,
    const DarkModeSettings& settings) {
  SkHighContrastConfig config;
  config.fInvertStyle = invert_style;
  config.fGrayscale = settings.grayscale;
  config.fContrast = settings.contrast;
  return SkHighContrastFilter::Make(config);
}

// Further darken dark grays to match the primary surface color recommended by
// the material design guidelines:
//   https://material.io/design/color/dark-theme.html#properties
//
// TODO(gilmanmh): Consider adding a more general way to adjust colors after
// applying the main filter.
void AdjustGray(Color* color) {
  DCHECK(color);
  static const int kBrightnessThreshold = 32;
  static const int kAdjustedBrightness = 18;

  if (color->Red() == color->Blue() && color->Red() == color->Green() &&
      color->Red() < kBrightnessThreshold &&
      color->Red() > kAdjustedBrightness) {
    color->SetRGB(kAdjustedBrightness, kAdjustedBrightness,
                  kAdjustedBrightness);
  }
}

class SkColorFilterWrapper : public DarkModeColorFilter {
 public:
  SkColorFilterWrapper(sk_sp<SkColorFilter> filter) : filter_(filter) {}

  Color InvertColor(const Color& color) const override {
    return Color(filter_->filterColor(color.Rgb()));
  }

  sk_sp<SkColorFilter> ToSkColorFilter() const override { return filter_; }

 private:
  sk_sp<SkColorFilter> filter_;
};

class LabColorFilter : public DarkModeColorFilter {
 public:
  LabColorFilter() : transformer_(LabColorSpace::RGBLABTransformer()) {
    SkHighContrastConfig config;
    config.fInvertStyle = SkHighContrastConfig::InvertStyle::kInvertLightness;
    config.fGrayscale = false;
    config.fContrast = 0.0;
    filter_ = SkHighContrastFilter::Make(config);
  }

  Color InvertColor(const Color& color) const override {
    blink::FloatPoint3D rgb = {color.Red() / 255.0f, color.Green() / 255.0f,
                               color.Blue() / 255.0f};
    blink::FloatPoint3D lab = transformer_.sRGBToLab(rgb);
    float invertedL = std::min(110.0f - lab.X(), 100.0f);
    lab.SetX(invertedL);
    rgb = transformer_.LabToSRGB(lab);

    Color inverted_color(Color(static_cast<unsigned int>(rgb.X() * 255 + 0.5),
                               static_cast<unsigned int>(rgb.Y() * 255 + 0.5),
                               static_cast<unsigned int>(rgb.Z() * 255 + 0.5),
                               color.Alpha()));
    AdjustGray(&inverted_color);
    return inverted_color;
  }

  sk_sp<SkColorFilter> ToSkColorFilter() const override { return filter_; }

 private:
  const LabColorSpace::RGBLABTransformer transformer_;
  sk_sp<SkColorFilter> filter_;
};

}  // namespace

std::unique_ptr<DarkModeColorFilter> DarkModeColorFilter::FromSettings(
    const DarkModeSettings& settings) {
  switch (settings.mode) {
    case DarkModeInversionAlgorithm::kOff:
      return nullptr;

    case DarkModeInversionAlgorithm::kSimpleInvertForTesting:
      uint8_t identity[256], invert[256];
      for (int i = 0; i < 256; ++i) {
        identity[i] = i;
        invert[i] = 255 - i;
      }
      return std::make_unique<SkColorFilterWrapper>(
          SkTableColorFilter::MakeARGB(identity, invert, invert, invert));

    case DarkModeInversionAlgorithm::kInvertBrightness:
      return std::make_unique<SkColorFilterWrapper>(SkColorFilterFromSettings(
          SkHighContrastConfig::InvertStyle::kInvertBrightness, settings));

    case DarkModeInversionAlgorithm::kInvertLightness:
      return std::make_unique<SkColorFilterWrapper>(SkColorFilterFromSettings(
          SkHighContrastConfig::InvertStyle::kInvertLightness, settings));

    case DarkModeInversionAlgorithm::kInvertLightnessLAB:
      return std::make_unique<LabColorFilter>();
  }
  NOTREACHED();
}

DarkModeColorFilter::~DarkModeColorFilter() {}

}  // namespace blink
