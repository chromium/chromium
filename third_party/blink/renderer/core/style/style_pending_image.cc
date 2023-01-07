// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/style/style_pending_image.h"

#include "third_party/blink/renderer/core/css/css_crossfade_value.h"
#include "third_party/blink/renderer/core/css/css_gradient_value.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_image_generator_value.h"
#include "third_party/blink/renderer/core/css/css_image_set_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_paint_value.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

class ComputedCSSValueBuilder {
  STACK_ALLOCATED();

 public:
  ComputedCSSValueBuilder(const ComputedStyle& style, bool allow_visited_style)
      : style_(style), allow_visited_style_(allow_visited_style) {}

  CSSValue* Build(CSSValue* value) const;

 private:
  CSSValue* CrossfadeArgument(CSSValue*) const;

  const ComputedStyle& style_;
  const bool allow_visited_style_;
};

CSSValue* ComputedCSSValueBuilder::CrossfadeArgument(CSSValue* value) const {
  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kNone);
    return value;
  }
  return Build(value);
}

CSSValue* ComputedCSSValueBuilder::Build(CSSValue* value) const {
  if (auto* image_value = DynamicTo<CSSImageValue>(value))
    return image_value->ValueWithURLMadeAbsolute();
  if (auto* image_set_value = DynamicTo<CSSImageSetValue>(value))
    return image_set_value->ValueWithURLsMadeAbsolute();
  if (auto* image_crossfade = DynamicTo<cssvalue::CSSCrossfadeValue>(value)) {
    return MakeGarbageCollected<cssvalue::CSSCrossfadeValue>(
        CrossfadeArgument(&image_crossfade->From()),
        CrossfadeArgument(&image_crossfade->To()),
        &image_crossfade->Percentage());
  }
  if (IsA<CSSPaintValue>(value))
    return value;
  if (auto* image_gradient_value = DynamicTo<cssvalue::CSSGradientValue>(value))
    return image_gradient_value->ComputedCSSValue(style_, allow_visited_style_);
  NOTREACHED();
  return value;
}

}  // namespace

CSSValue* StylePendingImage::ComputedCSSValue(const ComputedStyle& style,
                                              bool allow_visited_style) const {
  DCHECK(style.IsEnsuredInDisplayNone() ||
         style.Display() == EDisplay::kContents);
  return ComputedCSSValueBuilder(style, allow_visited_style).Build(CssValue());
}

}  // namespace blink
