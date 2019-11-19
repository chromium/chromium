// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_COLOR_CLASSIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_COLOR_CLASSIFIER_H_

#include <memory>

#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class PLATFORM_EXPORT DarkModeColorClassifier {
 public:
  // Determine perceived brightness of a color.
  static int CalculateColorBrightness(const Color& color);

  static std::unique_ptr<DarkModeColorClassifier> MakeTextColorClassifier(
      const DarkModeSettings& settings);
  static std::unique_ptr<DarkModeColorClassifier> MakeBackgroundColorClassifier(
      const DarkModeSettings& settings);

  virtual ~DarkModeColorClassifier();

  // TODO(https://crbug.com/968340): Include element opacity when determining
  // whether to invert a color. The background is likely to be dark, so a lower
  // opacity will usually decrease the effective brightness of both the original
  // and the inverted colors.
  virtual DarkModeClassification ShouldInvertColor(const Color& color) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_COLOR_CLASSIFIER_H_
