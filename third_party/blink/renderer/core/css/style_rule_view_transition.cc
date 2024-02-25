// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_view_transition.h"

#include "base/auto_reset.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"

namespace blink {

StyleRuleViewTransition::StyleRuleViewTransition(
    CSSPropertyValueSet& properties)
    : StyleRuleBase(kViewTransition),
      navigation_(properties.GetPropertyCSSValue(CSSPropertyID::kNavigation)) {}

StyleRuleViewTransition::StyleRuleViewTransition(
    const StyleRuleViewTransition&) = default;

StyleRuleViewTransition::~StyleRuleViewTransition() = default;

const CSSValue* StyleRuleViewTransition::GetNavigation() const {
  return navigation_.Get();
}

void StyleRuleViewTransition::SetNavigation(const CSSValue* new_value) {
  navigation_ = new_value;
}

void StyleRuleViewTransition::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(layer_);
  visitor->Trace(navigation_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
