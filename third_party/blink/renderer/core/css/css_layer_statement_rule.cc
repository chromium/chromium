// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_layer_statement_rule.h"

#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSLayerStatementRule::CSSLayerStatementRule(
    StyleRuleLayerStatement* layer_statement_rule,
    CSSStyleSheet* parent)
    : CSSRule(parent), layer_statement_rule_(layer_statement_rule) {}

CSSLayerStatementRule::~CSSLayerStatementRule() = default;

Vector<String> CSSLayerStatementRule::nameList() const {
  return layer_statement_rule_->GetNamesAsStrings();
}

String CSSLayerStatementRule::cssText() const {
  StringBuilder result;
  result.Append("@layer ");
  const Vector<String>& names = nameList();
  result.Append(names[0]);
  for (unsigned i = 1; i < names.size(); ++i) {
    result.Append(", ");
    result.Append(names[i]);
  }
  result.Append(';');
  return result.ReleaseString();
}

void CSSLayerStatementRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  layer_statement_rule_ = To<StyleRuleLayerStatement>(rule);
}

void CSSLayerStatementRule::Trace(Visitor* visitor) const {
  visitor->Trace(layer_statement_rule_);
  CSSRule::Trace(visitor);
}

}  // namespace blink
