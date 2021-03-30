// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_pending_image.h"

#include "third_party/blink/renderer/core/css/css_image_generator_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

CSSValue* StylePendingImage::ComputedCSSValue(const ComputedStyle& style,
                                              bool allow_visited_style) const {
  DCHECK(style.IsEnsuredInDisplayNone() ||
         style.Display() == EDisplay::kContents);

  CSSValue* value = CssValue();
  if (auto* image_value = DynamicTo<CSSImageValue>(value))
    return image_value->ValueWithURLMadeAbsolute();
  if (auto* image_set_value = DynamicTo<CSSImageSetValue>(value))
    return image_set_value->ValueWithURLsMadeAbsolute();
  if (auto* image_generator_value = DynamicTo<CSSImageGeneratorValue>(value))
    return image_generator_value->ComputedCSSValue(style, allow_visited_style);
  NOTREACHED();
  return value;
}

}  // namespace blink
