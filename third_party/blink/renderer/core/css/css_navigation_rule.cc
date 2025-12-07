// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_navigation_rule.h"

#include "third_party/blink/renderer/core/css/conditional_exp_node.h"
#include "third_party/blink/renderer/core/css/navigation_query.h"
#include "third_party/blink/renderer/core/css/style_rule.h"

namespace blink {

CSSNavigationRule::CSSNavigationRule(StyleRuleNavigation* navigation_rule,
                                     CSSStyleSheet* parent)
    : CSSConditionRule(navigation_rule, parent),
      navigation_rule_(navigation_rule) {}

CSSNavigationRule::~CSSNavigationRule() = default;

String CSSNavigationRule::cssText() const {
  StringBuilder result;
  result.Append("@navigation ");
  navigation_rule_->GetNavigationQuery().GetRootExp()->SerializeTo(result);
  AppendCSSTextForItems(result);
  return result.ToString();
}

void CSSNavigationRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  navigation_rule_ = To<StyleRuleNavigation>(rule);
  CSSConditionRule::Reattach(rule);
}

void CSSNavigationRule::Trace(Visitor* visitor) const {
  visitor->Trace(navigation_rule_);
  CSSConditionRule::Trace(visitor);
}

}  // namespace blink
