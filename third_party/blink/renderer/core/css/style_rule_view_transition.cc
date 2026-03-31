// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_view_transition.h"

#include "base/memory/values_equivalent.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/css_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"

namespace blink {

namespace {
Vector<String> ExtractTypesFromCSSValue(const CSSValue* types) {
  if (!types) {
    return Vector<String>();
  }

  const CSSValueList* list = To<CSSValueList>(types);
  Vector<String> result;
  for (const CSSValue* value : *list) {
    result.push_back(value->CssText());
  }
  return result;
}
}  // namespace

StyleRuleViewTransition::StyleRuleViewTransition(
    CSSPropertyValueSet& properties)
    : StyleRuleBase(kViewTransition),
      navigation_(properties.GetPropertyCSSValue(CSSPropertyID::kNavigation)),
      types_(ExtractTypesFromCSSValue(
          properties.GetPropertyCSSValue(CSSPropertyID::kTypes))) {}

StyleRuleViewTransition::StyleRuleViewTransition(
    const StyleRuleViewTransition&) = default;

StyleRuleViewTransition::~StyleRuleViewTransition() = default;

StyleRuleViewTransition::NavigationType StyleRuleViewTransition::GetNavigation()
    const {
  if (!navigation_) {
    return NavigationType::kUnspecified;
  }
  if (navigation_->IsIdentifierValue()) {
    const CSSIdentifierValue* identifier =
        To<CSSIdentifierValue>(navigation_.Get());
    switch (identifier->GetValueID()) {
      case CSSValueID::kNone:
        return NavigationType::kNone;
      case CSSValueID::kAuto:
        return NavigationType::kAuto;
      case CSSValueID::kPreview:
        return NavigationType::kPreview;
      default:
        NOTREACHED();
    }
  }
  return NavigationType::kAuto;
}

void StyleRuleViewTransition::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(navigation_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
