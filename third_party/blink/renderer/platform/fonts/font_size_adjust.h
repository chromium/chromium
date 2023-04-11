// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_SIZE_ADJUST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_SIZE_ADJUST_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class PLATFORM_EXPORT FontSizeAdjust {
 public:
  FontSizeAdjust() = default;
  explicit FontSizeAdjust(float value) : value_(value) {}

  static constexpr float kFontSizeAdjustNone = -1;

  float Value() const { return value_; }

  explicit operator bool() const { return value_ != kFontSizeAdjustNone; }
  bool operator==(const FontSizeAdjust& other) const {
    return value_ == other.Value();
  }
  bool operator!=(const FontSizeAdjust& other) const {
    return !operator==(other);
  }

 private:
  float value_{kFontSizeAdjustNone};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_FONT_SIZE_ADJUST_H_
