// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_property_rule.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSPropertyRule::CSSPropertyRule(StyleRuleProperty* property_rule,
                                 CSSStyleSheet* sheet)
    : CSSRule(sheet), property_rule_(property_rule) {}

CSSPropertyRule::~CSSPropertyRule() = default;

CSSStyleDeclaration* CSSPropertyRule::style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ =
        MakeGarbageCollected<StyleRuleCSSStyleDeclaration>(
            property_rule_->MutableProperties(),
            const_cast<CSSPropertyRule*>(this));
  }

  return properties_cssom_wrapper_.Get();
}

String CSSPropertyRule::cssText() const {
  // TODO(https://crbug.com/978783): Implement this.
  return "";
}

void CSSPropertyRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  property_rule_ = To<StyleRuleProperty>(rule);
  if (properties_cssom_wrapper_)
    properties_cssom_wrapper_->Reattach(property_rule_->MutableProperties());
}

void CSSPropertyRule::Trace(blink::Visitor* visitor) {
  visitor->Trace(property_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
