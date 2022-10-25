// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_scope_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSScopeRule::CSSScopeRule(StyleRuleScope* scope_rule, CSSStyleSheet* parent)
    : CSSGroupingRule(scope_rule, parent) {}

CSSScopeRule::~CSSScopeRule() = default;

String CSSScopeRule::PreludeText() const {
  StringBuilder result;
  const StyleScope& scope =
      To<StyleRuleScope>(*group_rule_.Get()).GetStyleScope();

  result.Append('(');
  result.Append(scope.From().SelectorsText());
  result.Append(')');

  if (scope.To()) {
    result.Append(" to (");
    result.Append(scope.To()->SelectorsText());
    result.Append(')');
  }

  return result.ReleaseString();
}

String CSSScopeRule::cssText() const {
  StringBuilder result;
  result.Append("@scope ");
  result.Append(PreludeText());
  AppendCSSTextForItems(result);
  return result.ReleaseString();
}

void CSSScopeRule::SetPreludeText(const ExecutionContext* execution_context,
                                  String value) {
  CSSStyleSheet::RuleMutationScope mutation_scope(this);
  To<StyleRuleScope>(group_rule_.Get())
      ->SetPreludeText(execution_context, value);
}

}  // namespace blink
