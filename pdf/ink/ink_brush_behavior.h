// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_INK_INK_BRUSH_BEHAVIOR_H_
#define PDF_INK_INK_BRUSH_BEHAVIOR_H_

#include <cstdint>
#include <vector>

namespace chrome_pdf {

struct InkBrushBehavior {
  struct EnabledToolTypes {
    bool unknown = false;
    bool mouse = false;
    bool touch = false;
    bool stylus = false;
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

  // TODO(crbug.com/339682315): Add more types if needed.
  enum class Type {
    kFallbackFilter,
    kToolTypeFilter,
  };

  // Deliberately avoid using absl::variant in this header.
  struct BaseNode {
    Type type;
  };

  struct FallbackFilterNode : public BaseNode {
    OptionalInputProperty is_fallback_for;
  };

  struct ToolTypeFilterNode : public BaseNode {
    EnabledToolTypes enabled_tool_types;
  };

  InkBrushBehavior();
  ~InkBrushBehavior();

  std::vector<BaseNode> nodes;
};

}  // namespace chrome_pdf

#endif  // PDF_INK_INK_BRUSH_BEHAVIOR_H_
