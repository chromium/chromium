// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_margin_rule.h"

#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSMarginRule::CSSMarginRule(StyleRulePageMargin* margin_rule,
                             CSSStyleSheet* parent)
    : CSSRule(parent), margin_rule_(margin_rule) {}

CSSStyleDeclaration* CSSMarginRule::style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ =
        MakeGarbageCollected<StyleRuleCSSStyleDeclaration>(
            margin_rule_->MutableProperties(),
            const_cast<CSSMarginRule*>(this));
  }
  return properties_cssom_wrapper_.Get();
}

String CSSMarginRule::name() const {
  // Return the name of the rule, without the preceding '@'.
  return StringView(CssAtRuleIDToString(margin_rule_->ID()), 1).ToString();
}

String CSSMarginRule::cssText() const {
  // TODO(mstensho): Serialization needs to be specced:
  // https://github.com/w3c/csswg-drafts/issues/9952
  StringBuilder result;
  result.Append(CssAtRuleIDToString(margin_rule_->ID()));
  result.Append(" { ");
  String decls = margin_rule_->Properties().AsText();
  result.Append(decls);
  if (!decls.empty()) {
    result.Append(' ');
  }
  result.Append("}");
  return result.ReleaseString();
}

void CSSMarginRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  margin_rule_ = To<StyleRulePageMargin>(rule);
  if (properties_cssom_wrapper_) {
    properties_cssom_wrapper_->Reattach(margin_rule_->MutableProperties());
  }
}

void CSSMarginRule::Trace(Visitor* visitor) const {
  visitor->Trace(margin_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
