// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_pending_image.h"

#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/style_image_computed_css_value_builder.h"

namespace blink {

CSSValue* StylePendingImage::ComputedCSSValue(const ComputedStyle& style,
                                              bool allow_visited_style,
                                              CSSValuePhase value_phase) const {
  DCHECK(style.IsEnsuredInDisplayNone() ||
         style.Display() == EDisplay::kContents);
  return StyleImageComputedCSSValueBuilder(style, allow_visited_style,
                                           value_phase)
      .Build(CssValue());
}

}  // namespace blink
