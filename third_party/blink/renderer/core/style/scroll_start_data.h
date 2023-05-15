// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SCROLL_START_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SCROLL_START_DATA_H_

#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

enum class ScrollStartValueType {
  kLengthOrPercentage,
  kStart,
  kCenter,
  kEnd,
  kAuto,
  kTop,
  kBottom,
  kLeft,
  kRight,
};

struct ScrollStartData {
  ScrollStartValueType value_type = ScrollStartValueType::kAuto;
  blink::Length value;

  bool operator==(const ScrollStartData& other) const {
    return value_type == other.value_type && value == other.value;
  }

  bool operator!=(const ScrollStartData& other) const {
    return !(*this == other);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_SCROLL_START_DATA_H_
