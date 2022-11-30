// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_try_rule.h"

#include "third_party/blink/renderer/core/css/css_position_fallback_rule.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StyleRuleTry::StyleRuleTry(CSSPropertyValueSet* properties)
    : StyleRuleBase(kTry), properties_(properties) {}

StyleRuleTry::~StyleRuleTry() = default;

void StyleRuleTry::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

CSSTryRule::CSSTryRule(StyleRuleTry* try_rule, CSSPositionFallbackRule* parent)
    : CSSRule(nullptr), try_rule_(try_rule) {
  SetParentRule(parent);
}

CSSTryRule::~CSSTryRule() = default;

String CSSTryRule::cssText() const {
  StringBuilder result;
  result.Append("@try { ");
  if (!try_rule_->Properties().IsEmpty()) {
    result.Append(try_rule_->Properties().AsText());
    result.Append(" ");
  }
  result.Append("}");
  return result.ReleaseString();
}

void CSSTryRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  try_rule_ = To<StyleRuleTry>(rule);
}

void CSSTryRule::Trace(Visitor* visitor) const {
  visitor->Trace(try_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
