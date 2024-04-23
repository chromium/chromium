// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_BRUSH_BEHAVIOR_H_
#define PDF_INK_INK_BRUSH_BEHAVIOR_H_

#include <cstdint>
#include <optional>

namespace chrome_pdf {

struct InkBrushBehavior {
  struct EnabledToolTypes {
    bool operator==(const EnabledToolTypes& other) const;
    bool operator!=(const EnabledToolTypes& other) const;

    bool unknown = false;
    bool mouse = false;
    bool touch = false;
    bool stylus = false;

    bool HasAnyTypes() const;
    bool HasAllTypes() const;
  };

  static constexpr EnabledToolTypes kAllToolTypes = {.unknown = true,
                                                     .mouse = true,
                                                     .touch = true,
                                                     .stylus = true};

  enum OptionalInputProperty : int8_t {
    kPressure,
    kTilt,
    kOrientation,
    kTiltXAndY,
  };

  bool operator==(const InkBrushBehavior& other) const;
  bool operator!=(const InkBrushBehavior& other) const;

  float response_time_seconds;
  EnabledToolTypes enabled_tool_types = kAllToolTypes;
  std::optional<OptionalInputProperty> is_fallback_for;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_BRUSH_BEHAVIOR_H_
