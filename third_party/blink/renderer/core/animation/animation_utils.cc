// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/animation_utils.h"

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_property_ref.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

const CSSValue* AnimationUtils::KeyframeValueFromComputedStyle(
    const PropertyHandle& property,
    const ComputedStyle& style,
    const Document& document,
    const LayoutObject* layout_object) {
  if (property.IsCSSCustomProperty()) {
    // Holds registration and creates temporary CSSProperty.
    CSSPropertyRef custom_ref(property.GetCSSPropertyName(), document);
    return ComputedStyleUtils::ComputedPropertyValue(custom_ref.GetProperty(),
                                                     style, layout_object);
  }

  return ComputedStyleUtils::ComputedPropertyValue(property.GetCSSProperty(),
                                                   style, layout_object);
}

void AnimationUtils::ForEachInterpolatedPropertyValue(
    Element* target,
    const PropertyHandleSet& properties,
    ActiveInterpolationsMap& interpolations,
    base::FunctionRef<void(PropertyHandle, const CSSValue*)> callback) {
  if (!target)
    return;

  StyleResolver& resolver = target->GetDocument().GetStyleResolver();
  const ComputedStyle* style =
      resolver.StyleForInterpolations(*target, interpolations);

  for (const auto& property : properties) {
    if (!property.IsCSSProperty())
      continue;

    const CSSValue* value = KeyframeValueFromComputedStyle(
        property, *style, target->GetDocument(), target->GetLayoutObject());
    if (!value)
      continue;

    callback(property, value);
  }
}

}  // namespace blink
