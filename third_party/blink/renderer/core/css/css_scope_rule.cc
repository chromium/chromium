// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_scope_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
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

  if (!scope.IsImplicit()) {
    result.Append('(');
    result.Append(CSSSelectorList::SelectorsText(scope.From()));
    result.Append(')');
  }

  if (scope.To()) {
    if (!result.empty()) {
      result.Append(" ");
    }
    result.Append("to (");
    result.Append(CSSSelectorList::SelectorsText(scope.To()));
    result.Append(')');
  }

  return result.ReleaseString();
}

String CSSScopeRule::cssText() const {
  StringBuilder result;
  result.Append("@scope");
  String prelude = PreludeText();
  if (!prelude.empty()) {
    result.Append(" ");
    result.Append(prelude);
  }
  AppendCSSTextForItems(result);
  return result.ReleaseString();
}

String CSSScopeRule::start() const {
  const StyleScope& scope =
      To<StyleRuleScope>(*group_rule_.Get()).GetStyleScope();
  return scope.From() ? CSSSelectorList::SelectorsText(scope.From())
                      : g_null_atom;
}

String CSSScopeRule::end() const {
  const StyleScope& scope =
      To<StyleRuleScope>(*group_rule_.Get()).GetStyleScope();
  return scope.To() ? CSSSelectorList::SelectorsText(scope.To()) : g_null_atom;
}

void CSSScopeRule::SetPreludeText(const ExecutionContext* execution_context,
                                  String value) {
  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  // Find enclosing style rule or @scope rule, whichever comes first:
  CSSNestingType nesting_type = CSSNestingType::kNone;
  StyleRule* parent_rule_for_nesting = nullptr;
  bool is_within_scope = false;
  for (CSSRule* parent = parentRule(); parent; parent = parent->parentRule()) {
    if (const auto* style_rule = DynamicTo<CSSStyleRule>(parent)) {
      if (nesting_type == CSSNestingType::kNone) {
        nesting_type = CSSNestingType::kNesting;
        parent_rule_for_nesting = style_rule->GetStyleRule();
      }
    }
    if (const auto* scope_rule = DynamicTo<CSSScopeRule>(parent)) {
      if (nesting_type == CSSNestingType::kNone) {
        nesting_type = CSSNestingType::kScope;
        parent_rule_for_nesting =
            scope_rule->GetStyleRuleScope().GetStyleScope().RuleForNesting();
      }
      is_within_scope = true;
    }
  }

  CSSStyleSheet* style_sheet = parentStyleSheet();
  StyleSheetContents* contents =
      style_sheet ? style_sheet->Contents() : nullptr;

  GetStyleRuleScope().SetPreludeText(execution_context, value, nesting_type,
                                     parent_rule_for_nesting, is_within_scope,
                                     contents);
}

StyleRuleScope& CSSScopeRule::GetStyleRuleScope() {
  return *To<StyleRuleScope>(group_rule_.Get());
}

const StyleRuleScope& CSSScopeRule::GetStyleRuleScope() const {
  return *To<StyleRuleScope>(group_rule_.Get());
}

}  // namespace blink
