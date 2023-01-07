// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_color_classifier.h"

#include "base/check_op.h"

namespace blink {
namespace {

class SimpleColorClassifier : public DarkModeColorClassifier {
 public:
  static std::unique_ptr<SimpleColorClassifier> NeverInvert() {
    return std::unique_ptr<SimpleColorClassifier>(
        new SimpleColorClassifier(DarkModeResult::kDoNotApplyFilter));
  }

  static std::unique_ptr<SimpleColorClassifier> AlwaysInvert() {
    return std::unique_ptr<SimpleColorClassifier>(
        new SimpleColorClassifier(DarkModeResult::kApplyFilter));
  }

  DarkModeResult ShouldInvertColor(SkColor color) override { return value_; }

 private:
  explicit SimpleColorClassifier(DarkModeResult value) : value_(value) {}

  DarkModeResult value_;
};

class InvertLowBrightnessColorsClassifier : public DarkModeColorClassifier {
 public:
  explicit InvertLowBrightnessColorsClassifier(int brightness_threshold)
      : brightness_threshold_(brightness_threshold) {
    DCHECK_GT(brightness_threshold_, 0);
    DCHECK_LT(brightness_threshold_, 255);
  }

  DarkModeResult ShouldInvertColor(SkColor color) override {
    if (CalculateColorBrightness(color) < brightness_threshold_)
      return DarkModeResult::kApplyFilter;
    return DarkModeResult::kDoNotApplyFilter;
  }

 private:
  int brightness_threshold_;
};

class InvertHighBrightnessColorsClassifier : public DarkModeColorClassifier {
 public:
  explicit InvertHighBrightnessColorsClassifier(int brightness_threshold)
      : brightness_threshold_(brightness_threshold) {
    DCHECK_GT(brightness_threshold_, 0);
    DCHECK_LT(brightness_threshold_, 255);
  }

  DarkModeResult ShouldInvertColor(SkColor color) override {
    if (CalculateColorBrightness(color) > brightness_threshold_)
      return DarkModeResult::kApplyFilter;
    return DarkModeResult::kDoNotApplyFilter;
  }

 private:
  int brightness_threshold_;
};

}  // namespace

// Based on this algorithm suggested by the W3:
// https://www.w3.org/TR/AERT/#color-contrast
//
// We don't use HSL or HSV here because perceived brightness is a function of
// hue as well as lightness/value.
int DarkModeColorClassifier::CalculateColorBrightness(SkColor color) {
  int weighted_red = SkColorGetR(color) * 299;
  int weighted_green = SkColorGetG(color) * 587;
  int weighted_blue = SkColorGetB(color) * 114;
  return (weighted_red + weighted_green + weighted_blue) / 1000;
}

std::unique_ptr<DarkModeColorClassifier>
DarkModeColorClassifier::MakeForegroundColorClassifier(
    const DarkModeSettings& settings) {
  DCHECK_LE(settings.foreground_brightness_threshold, 255);
  DCHECK_GE(settings.foreground_brightness_threshold, 0);

  // The value should be between 0 and 255, but check for values outside that
  // range here to preserve correct behavior in non-debug builds.
  if (settings.foreground_brightness_threshold >= 255)
    return SimpleColorClassifier::AlwaysInvert();
  if (settings.foreground_brightness_threshold <= 0)
    return SimpleColorClassifier::NeverInvert();

  return std::make_unique<InvertLowBrightnessColorsClassifier>(
      settings.foreground_brightness_threshold);
}

std::unique_ptr<DarkModeColorClassifier>
DarkModeColorClassifier::MakeBackgroundColorClassifier(
    const DarkModeSettings& settings) {
  DCHECK_LE(settings.background_brightness_threshold, 255);
  DCHECK_GE(settings.background_brightness_threshold, 0);

  // The value should be between 0 and 255, but check for values outside that
  // range here to preserve correct behavior in non-debug builds.
  if (settings.background_brightness_threshold >= 255)
    return SimpleColorClassifier::NeverInvert();
  if (settings.background_brightness_threshold <= 0)
    return SimpleColorClassifier::AlwaysInvert();

  return std::make_unique<InvertHighBrightnessColorsClassifier>(
      settings.background_brightness_threshold);
}

DarkModeColorClassifier::~DarkModeColorClassifier() {}

}  // namespace blink
