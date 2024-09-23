// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/properties/css_property.h"

#include "third_party/blink/renderer/core/css/cssom/cross_thread_unsupported_value.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands/variable.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {

const CSSProperty& GetCSSPropertyVariable() {
  return To<CSSProperty>(*GetPropertyInternal(CSSPropertyID::kVariable));
}

bool CSSProperty::HasEqualCSSPropertyName(const CSSProperty& other) const {
  return property_id_ == other.property_id_;
}

// The correctness of static functions that operate on CSSPropertyName is
// ensured by:
//
// - DCHECKs in the CustomProperty constructor.
// - CSSPropertyTest.StaticVariableInstanceFlags

bool CSSProperty::IsShorthand(const CSSPropertyName& name) {
  return !name.IsCustomProperty() && Get(name.Id()).IsShorthand();
}

bool CSSProperty::IsRepeated(const CSSPropertyName& name) {
  return !name.IsCustomProperty() && Get(name.Id()).IsRepeated();
}

std::unique_ptr<CrossThreadStyleValue>
CSSProperty::CrossThreadStyleValueFromComputedStyle(
    const ComputedStyle& computed_style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const CSSValue* css_value = CSSValueFromComputedStyle(
      computed_style, layout_object, allow_visited_style, value_phase);
  if (!css_value) {
    return std::make_unique<CrossThreadUnsupportedValue>("");
  }
  CSSStyleValue* style_value =
      StyleValueFactory::CssValueToStyleValue(GetCSSPropertyName(), *css_value);
  if (!style_value) {
    return std::make_unique<CrossThreadUnsupportedValue>("");
  }
  return ComputedStyleUtils::CrossThreadStyleValueFromCSSStyleValue(
      style_value);
}

const CSSValue* CSSProperty::CSSValueFromComputedStyle(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    bool allow_visited_style,
    CSSValuePhase value_phase) const {
  const CSSProperty& resolved_property =
      ResolveDirectionAwareProperty(style.GetWritingDirection());
  return resolved_property.CSSValueFromComputedStyleInternal(
      style, layout_object, allow_visited_style, value_phase);
}

void CSSProperty::FilterWebExposedCSSPropertiesIntoVector(
    const ExecutionContext* execution_context,
    const CSSPropertyID* properties,
    wtf_size_t property_count,
    Vector<const CSSProperty*>& out_vector) {
  DCHECK(out_vector.empty());
  out_vector.reserve(property_count);
  for (unsigned i = 0; i < property_count; i++) {
    const CSSProperty& property = Get(properties[i]);
    if (property.IsWebExposed(execution_context)) {
      out_vector.push_back(&property);
    }
  }
}

}  // namespace blink
