// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_view_transition_rule.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/at_rule_descriptor_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_tokenizer.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_view_transition.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSViewTransitionRule::CSSViewTransitionRule(
    StyleRuleViewTransition* initial_rule,
    CSSStyleSheet* parent)
    : CSSRule(parent), view_transition_rule_(initial_rule) {}

String CSSViewTransitionRule::cssText() const {
  StringBuilder result;

  result.Append("@view-transition { ");

  String navigation_value = navigation();
  if (!navigation_value.empty()) {
    result.Append("navigation: ");
    result.Append(navigation_value);
    result.Append("; ");
  }

  result.Append("}");

  return result.ReleaseString();
}

String CSSViewTransitionRule::navigation() const {
  if (const CSSValue* value = view_transition_rule_->GetNavigation()) {
    return value->CssText();
  }

  return String();
}

Vector<String> CSSViewTransitionRule::types() const {
  return view_transition_rule_->GetTypes();
}

void CSSViewTransitionRule::Reattach(StyleRuleBase* rule) {
  CHECK(rule);
  view_transition_rule_ = To<StyleRuleViewTransition>(rule);
}

void CSSViewTransitionRule::Trace(Visitor* visitor) const {
  visitor->Trace(view_transition_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
