// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_keyframe_shorthand_value.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {

#if DCHECK_IS_ON()
namespace {
bool ShorthandMatches(CSSPropertyID expected_shorthand,
                      CSSPropertyID longhand) {
  Vector<StylePropertyShorthand, 4> shorthands;
  getMatchingShorthandsForLonghand(longhand, &shorthands);
  for (unsigned i = 0; i < shorthands.size(); ++i) {
    if (shorthands.at(i).id() == expected_shorthand)
      return true;
  }

  return false;
}

}  // namespace
#endif

CSSKeyframeShorthandValue::CSSKeyframeShorthandValue(
    CSSPropertyID shorthand,
    ImmutableCSSPropertyValueSet* properties)
    : CSSValue(kKeyframeShorthandClass),
      shorthand_(shorthand),
      properties_(properties) {}

String CSSKeyframeShorthandValue::CustomCSSText() const {
#if DCHECK_IS_ON()
  // Check that all property/value pairs belong to the same shorthand.
  for (unsigned i = 0; i < properties_->PropertyCount(); i++) {
    DCHECK(ShorthandMatches(shorthand_, properties_->PropertyAt(i).Id()))
        << "These are not the longhands you're looking for.";
  }
#endif

  return properties_->GetPropertyValue(shorthand_);
}

void CSSKeyframeShorthandValue::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(properties_);
  CSSValue::TraceAfterDispatch(visitor);
}

}  // namespace blink
