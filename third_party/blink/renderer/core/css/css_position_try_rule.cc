// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_position_try_rule.h"

#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/css_position_try_descriptors.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StyleRulePositionTry::StyleRulePositionTry(const AtomicString& name,
                                           CSSPropertyValueSet* properties)
    : StyleRuleBase(kPositionTry), name_(name), properties_(properties) {}

StyleRulePositionTry::~StyleRulePositionTry() = default;

void StyleRulePositionTry::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(layer_);
  visitor->Trace(properties_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

CSSPositionTryRule::CSSPositionTryRule(StyleRulePositionTry* position_try_rule,
                                       CSSStyleSheet* parent)
    : CSSRule(parent), position_try_rule_(position_try_rule) {}

CSSPositionTryRule::~CSSPositionTryRule() = default;

String CSSPositionTryRule::cssText() const {
  StringBuilder result;
  result.Append("@position-try ");
  result.Append(name());
  result.Append(" { ");
  if (!position_try_rule_->Properties().IsEmpty()) {
    result.Append(position_try_rule_->Properties().AsText());
    result.Append(" ");
  }
  result.Append("}");
  return result.ReleaseString();
}

MutableCSSPropertyValueSet& StyleRulePositionTry::MutableProperties() {
  if (!properties_->IsMutable()) {
    properties_ = properties_->MutableCopy();
  }
  return *To<MutableCSSPropertyValueSet>(properties_.Get());
}

CSSStyleDeclaration* CSSPositionTryRule::style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ = MakeGarbageCollected<CSSPositionTryDescriptors>(
        position_try_rule_->MutableProperties(),
        const_cast<CSSPositionTryRule*>(this));
  }
  return properties_cssom_wrapper_.Get();
}

void CSSPositionTryRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  position_try_rule_ = To<StyleRulePositionTry>(rule);
}

void CSSPositionTryRule::Trace(Visitor* visitor) const {
  visitor->Trace(position_try_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
