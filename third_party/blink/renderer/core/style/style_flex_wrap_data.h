// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_FLEX_WRAP_DATA_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_FLEX_WRAP_DATA_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class StyleFlexWrapData {
  DISALLOW_NEW();

 public:
  explicit StyleFlexWrapData(FlexWrapMode wrap_mode)
      : StyleFlexWrapData(wrap_mode,
                          /*is_balanced=*/false,
                          /*min_line_count=*/1u) {}
  StyleFlexWrapData(FlexWrapMode wrap_mode,
                    bool is_balanced,
                    uint16_t min_line_count)
      : wrap_mode_(static_cast<uint8_t>(wrap_mode)),
        is_balanced_(is_balanced),
        min_line_count_(min_line_count) {}

  FlexWrapMode GetWrapMode() const {
    return static_cast<FlexWrapMode>(wrap_mode_);
  }
  bool IsBalanced() const { return is_balanced_; }
  uint16_t MinLineCount() const { return min_line_count_; }

  bool operator==(const StyleFlexWrapData& o) const {
    return wrap_mode_ == o.wrap_mode_ && is_balanced_ == o.is_balanced_ &&
           min_line_count_ == o.min_line_count_;
  }

 private:
  uint16_t wrap_mode_ : 2;  // FlexWrapMode
  uint16_t is_balanced_ : 1;
  uint16_t min_line_count_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_STYLE_FLEX_WRAP_DATA_H_
