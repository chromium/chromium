// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_position_fallback_rule.h"

#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_try_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StyleRulePositionFallback::StyleRulePositionFallback(const AtomicString& name)
    : StyleRuleBase(kPositionFallback), name_(name) {}

StyleRulePositionFallback::StyleRulePositionFallback(
    const StyleRulePositionFallback&) = default;

StyleRulePositionFallback::~StyleRulePositionFallback() = default;

void StyleRulePositionFallback::ParserAppendTryRule(StyleRuleTry* try_rule) {
  try_rules_.push_back(try_rule);
}

void StyleRulePositionFallback::TraceAfterDispatch(Visitor* visitor) const {
  visitor->Trace(try_rules_);
  visitor->Trace(layer_);
  StyleRuleBase::TraceAfterDispatch(visitor);
}

CSSPositionFallbackRule::CSSPositionFallbackRule(
    StyleRulePositionFallback* position_fallback_rule,
    CSSStyleSheet* parent)
    : CSSRule(parent),
      position_fallback_rule_(position_fallback_rule),
      rule_list_cssom_wrapper_(
          MakeGarbageCollected<LiveCSSRuleList<CSSPositionFallbackRule>>(
              this)) {
  CreateChildRuleWrappers();
}

CSSPositionFallbackRule::~CSSPositionFallbackRule() = default;

void CSSPositionFallbackRule::CreateChildRuleWrappers() {
  child_rule_cssom_wrappers_.clear();
  child_rule_cssom_wrappers_.reserve(
      position_fallback_rule_->TryRules().size());
  for (StyleRuleTry* try_rule : position_fallback_rule_->TryRules()) {
    child_rule_cssom_wrappers_.push_back(
        MakeGarbageCollected<CSSTryRule>(try_rule, this));
  }
}

String CSSPositionFallbackRule::cssText() const {
  StringBuilder result;
  result.Append("@position-fallback ");
  result.Append(name());
  result.Append(" {\n");
  for (const CSSTryRule* child_rule : child_rule_cssom_wrappers_) {
    result.Append("  ");
    result.Append(child_rule->cssText());
    result.Append("\n");
  }
  result.Append("}");
  return result.ReleaseString();
}

void CSSPositionFallbackRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  position_fallback_rule_ = To<StyleRulePositionFallback>(rule);
  CreateChildRuleWrappers();
}

void CSSPositionFallbackRule::Trace(Visitor* visitor) const {
  visitor->Trace(position_fallback_rule_);
  visitor->Trace(child_rule_cssom_wrappers_);
  visitor->Trace(rule_list_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
