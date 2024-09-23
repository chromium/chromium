// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_nested_declarations_rule.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/style_rule_nested_declarations.h"

namespace blink {

CSSNestedDeclarationsRule::CSSNestedDeclarationsRule(
    StyleRuleNestedDeclarations* nested_declarations_rule,
    CSSStyleSheet* parent)
    : CSSRule(parent), nested_declarations_rule_(nested_declarations_rule) {}

CSSStyleDeclaration* CSSNestedDeclarationsRule::style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ =
        MakeGarbageCollected<StyleRuleCSSStyleDeclaration>(
            nested_declarations_rule_->MutableProperties(),
            const_cast<CSSNestedDeclarationsRule*>(this));
  }
  return properties_cssom_wrapper_.Get();
}

String CSSNestedDeclarationsRule::cssText() const {
  // "The CSSNestedDeclarations rule serializes as if its declaration block
  //  had been serialized directly".
  // https://drafts.csswg.org/css-nesting-1/#the-cssnestrule
  return nested_declarations_rule_->Properties().AsText();
}

void CSSNestedDeclarationsRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  nested_declarations_rule_ = To<StyleRuleNestedDeclarations>(rule);
  if (properties_cssom_wrapper_) {
    properties_cssom_wrapper_->Reattach(
        nested_declarations_rule_->MutableProperties());
  }
  if (style_rule_cssom_wrapper_) {
    style_rule_cssom_wrapper_->Reattach(
        nested_declarations_rule_->InnerStyleRule());
  }
}

CSSRule* CSSNestedDeclarationsRule::InnerCSSStyleRule() const {
  if (!style_rule_cssom_wrapper_) {
    style_rule_cssom_wrapper_ =
        nested_declarations_rule_->InnerStyleRule()->CreateCSSOMWrapper(
            /* position_hint */ std::numeric_limits<wtf_size_t>::max(),
            parentRule());
  }
  return style_rule_cssom_wrapper_.Get();
}

void CSSNestedDeclarationsRule::Trace(Visitor* visitor) const {
  visitor->Trace(nested_declarations_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  visitor->Trace(style_rule_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
