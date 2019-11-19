// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_

namespace blink {

enum class DarkModeInversionAlgorithm {
  // Default, drawing is unfiltered.
  // TODO(https://crbug.com/1002664): This value is deprecated and in the
  // process of being removed.
  kOff,
  // For testing only, does a simple 8-bit invert of every RGB pixel component.
  kSimpleInvertForTesting,
  kInvertBrightness,
  kInvertLightness,
  kInvertLightnessLAB,
};

enum class DarkModeImagePolicy {
  // Apply dark-mode filter to all images.
  kFilterAll,
  // Never apply dark-mode filter to any images.
  kFilterNone,
  // Apply dark-mode based on image content.
  kFilterSmart,
};

enum class DarkModePagePolicy {
  // Apply dark-mode filter to all frames, regardless of content.
  kFilterAll,
  // Apply dark-mode filter to frames based on background color.
  kFilterByBackground,
};

enum class DarkModeClassifierType {
  kIcon,
  kGeneric,
};

// New variables added to this struct should also be added to
// BuildDarkModeSettings() in
//   //src/third_party/blink/renderer/core/accessibility/apply_dark_mode.h
struct DarkModeSettings {
  DarkModeInversionAlgorithm mode = DarkModeInversionAlgorithm::kOff;
  bool grayscale = false;
  float image_grayscale_percent = 0.0;  // Valid range from 0.0 to 1.0
  float contrast = 0.0;                 // Valid range from -1.0 to 1.0
  DarkModeImagePolicy image_policy = DarkModeImagePolicy::kFilterNone;
  DarkModeClassifierType classifier_type = DarkModeClassifierType::kGeneric;

  // Text colors with brightness below this threshold will be inverted, and
  // above it will be left as in the original, non-dark-mode page.  Set to 256
  // to always invert text color or to 0 to never invert text color.
  int text_brightness_threshold = 256;

  // Background elements with brightness above this threshold will be inverted,
  // and below it will be left as in the original, non-dark-mode page.  Set to
  // 256 to never invert the color or to 0 to always invert it.
  //
  // Warning: This behavior is the opposite of text_brightness_threshold!
  int background_brightness_threshold = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_SETTINGS_H_
