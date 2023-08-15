// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_view_transitions_rule.h"

#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_view_transitions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSViewTransitionsRule::CSSViewTransitionsRule(
    StyleRuleViewTransitions* initial_rule,
    CSSStyleSheet* parent)
    : CSSRule(parent), view_transitions_rule_(initial_rule) {}

String CSSViewTransitionsRule::cssText() const {
  StringBuilder result;

  result.Append("@view-transitions { ");

  String navigation_trigger = navigationTrigger();
  if (!navigation_trigger.empty()) {
    result.Append("navigation-trigger: ");
    result.Append(navigation_trigger);
    result.Append("; ");
  }

  result.Append("}");

  return result.ReleaseString();
}

String CSSViewTransitionsRule::navigationTrigger() const {
  if (const CSSValue* value = view_transitions_rule_->GetNavigationTrigger()) {
    return value->CssText();
  }

  return String();
}

void CSSViewTransitionsRule::setNavigationTrigger(const ExecutionContext*,
                                                  const String&) {
  // TODO(crbug.com/1463966): Implement
}

void CSSViewTransitionsRule::Reattach(StyleRuleBase* rule) {
  CHECK(rule);
  view_transitions_rule_ = To<StyleRuleViewTransitions>(rule);
}

void CSSViewTransitionsRule::Trace(Visitor* visitor) const {
  visitor->Trace(view_transitions_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
