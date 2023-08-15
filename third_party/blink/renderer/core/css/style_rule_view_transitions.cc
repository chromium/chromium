// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_rule_view_transitions.h"

#include "base/auto_reset.h"
#include "base/memory/values_equivalent.h"
#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"

namespace blink {

StyleRuleViewTransitions::StyleRuleViewTransitions(
    CSSPropertyValueSet& properties)
    : StyleRuleBase(kViewTransitions),
      navigation_trigger_(
          properties.GetPropertyCSSValue(CSSPropertyID::kNavigationTrigger)) {}

StyleRuleViewTransitions::StyleRuleViewTransitions(
    const StyleRuleViewTransitions&) = default;

StyleRuleViewTransitions::~StyleRuleViewTransitions() = default;

const CSSValue* StyleRuleViewTransitions::GetNavigationTrigger() const {
  return navigation_trigger_;
}

void StyleRuleViewTransitions::TraceAfterDispatch(
    blink::Visitor* visitor) const {
  visitor->Trace(navigation_trigger_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

}  // namespace blink
