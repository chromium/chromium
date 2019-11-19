// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unsupported_value.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/svg_computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {

const CSSProperty& GetCSSPropertyVariable() {
  return To<CSSProperty>(GetCSSPropertyVariableInternal());
}

const CSSProperty& CSSProperty::Get(CSSPropertyID id) {
  DCHECK_NE(id, CSSPropertyID::kInvalid);
  DCHECK_LE(id, lastCSSProperty);  // last property id
  return To<CSSProperty>(CSSUnresolvedProperty::GetNonAliasProperty(id));
}

std::unique_ptr<CrossThreadStyleValue>
CSSProperty::CrossThreadStyleValueFromComputedStyle(
    const ComputedStyle& computed_style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const CSSValue* css_value = CSSValueFromComputedStyle(
      computed_style, layout_object, allow_visited_style);
  if (!css_value)
    return std::make_unique<CrossThreadUnsupportedValue>("");
  CSSStyleValue* style_value =
      StyleValueFactory::CssValueToStyleValue(GetCSSPropertyName(), *css_value);
  if (!style_value)
    return std::make_unique<CrossThreadUnsupportedValue>("");
  return ComputedStyleUtils::CrossThreadStyleValueFromCSSStyleValue(
      style_value);
}

const CSSValue* CSSProperty::CSSValueFromComputedStyle(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style) const {
  const SVGComputedStyle& svg_style = style.SvgStyle();
  const CSSProperty& resolved_property =
      ResolveDirectionAwareProperty(style.Direction(), style.GetWritingMode());
  return resolved_property.CSSValueFromComputedStyleInternal(
      style, svg_style, layout_object, allow_visited_style);
}

void CSSProperty::FilterWebExposedCSSPropertiesIntoVector(
    const CSSPropertyID* properties,
    size_t propertyCount,
    Vector<const CSSProperty*>& outVector) {
  for (unsigned i = 0; i < propertyCount; i++) {
    const CSSProperty& property = Get(properties[i]);
    if (property.IsWebExposed())
      outVector.push_back(&property);
  }
}

}  // namespace blink
