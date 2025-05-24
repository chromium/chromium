// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_function_declarations_rule.h"

#include "third_party/blink/renderer/core/css/css_function_descriptors.h"
#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/style_rule_function_declarations.h"

namespace blink {

CSSFunctionDeclarationsRule::CSSFunctionDeclarationsRule(
    StyleRuleFunctionDeclarations* function_declarations_rule,
    CSSStyleSheet* parent)
    : CSSRule(parent),
      function_declarations_rule_(function_declarations_rule) {}

CSSFunctionDescriptors* CSSFunctionDeclarationsRule::style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ = MakeGarbageCollected<CSSFunctionDescriptors>(
        function_declarations_rule_->MutableProperties(),
        const_cast<CSSFunctionDeclarationsRule*>(this));
  }
  return properties_cssom_wrapper_.Get();
}

String CSSFunctionDeclarationsRule::cssText() const {
  return function_declarations_rule_->Properties().AsText();
}

void CSSFunctionDeclarationsRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  function_declarations_rule_ = To<StyleRuleFunctionDeclarations>(rule);
  if (properties_cssom_wrapper_) {
    properties_cssom_wrapper_->Reattach(
        function_declarations_rule_->MutableProperties());
  }
}

void CSSFunctionDeclarationsRule::Trace(Visitor* visitor) const {
  visitor->Trace(function_declarations_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
