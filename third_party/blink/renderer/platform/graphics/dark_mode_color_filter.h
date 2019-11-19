// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_COLOR_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_COLOR_FILTER_H_

#include <memory>

#include "third_party/blink/renderer/platform/graphics/color.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/lab_color_space.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkColorFilter;

namespace blink {

// Contains logic specific to modifying colors drawn when dark mode is active.
class PLATFORM_EXPORT DarkModeColorFilter {
 public:
  static std::unique_ptr<DarkModeColorFilter> FromSettings(
      const DarkModeSettings& settings);

  virtual ~DarkModeColorFilter();
  virtual Color InvertColor(const Color& color) const = 0;
  virtual sk_sp<SkColorFilter> ToSkColorFilter() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DARK_MODE_COLOR_FILTER_H_
