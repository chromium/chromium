// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/collapsed_border_value.h"

#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

CollapsedBorderValue::CollapsedBorderValue(const BorderValue& border,
                                           const Color& color,
                                           EBorderPrecedence precedence)
    : CollapsedBorderValue(border.Style(), border.Width(), color, precedence) {}

CollapsedBorderValue::CollapsedBorderValue(EBorderStyle style,
                                           const LayoutUnit width,
                                           const Color& color,
                                           EBorderPrecedence precedence)
    : color_(color),
      style_(static_cast<unsigned>(style)),
      precedence_(precedence) {
  if (!ComputedStyle::BorderStyleIsVisible(style)) {
    width_ = 0;
  } else {
    if (width > 0 && width <= 1)
      width_ = 1;
    else
      width_ = width.ToUnsigned();
  }
  DCHECK(precedence != kBorderPrecedenceOff);
}

}  // namespace blink
