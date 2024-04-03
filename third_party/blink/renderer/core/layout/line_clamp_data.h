// Copyright 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_

#include <optional>

#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

struct LineClampData {
  DISALLOW_NEW();

  enum State {
    kDisabled,
    kEnabled,
    // The line-clamp context is enabled, but no forced truncation will happen.
    // This is different from kDisabled in that `text-overflow: ellipsis` will
    // not take effect inside it.
    kDontTruncate,
  };

  bool IsLineClampContext() const { return state != kDisabled; }

  std::optional<int> LinesUntilClamp() const {
    if (state == kEnabled) {
      return lines_until_clamp;
    }
    return std::optional<int>();
  }

  bool IsAtClampPoint() const {
    return state == kEnabled && lines_until_clamp == 1;
  }

  bool IsPastClampPoint() const {
    return state == kEnabled && lines_until_clamp <= 0;
  }

  bool ShouldHideForPaint() const {
    return RuntimeEnabledFeatures::CSSLineClampEnabled() && IsPastClampPoint();
  }

  bool operator==(const LineClampData& other) const {
    if (state != other.state) {
      return false;
    }
    if (state == kEnabled) {
      return lines_until_clamp == other.lines_until_clamp;
    }
    return true;
  }

  // If state == kEnabled, the number of lines until a clamp. A value of 1
  // indicates the current line should be clamped. This may go negative.
  int lines_until_clamp;

  State state = kDisabled;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_LINE_CLAMP_DATA_H_
