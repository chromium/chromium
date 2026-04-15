// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/css_scope_rule.h"

#include "third_party/blink/renderer/core/css/css_markup.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
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
  for (CSSRule* parent = parentRule(); parent; parent = parent->parentRule()) {
    if (const auto* style_rule = DynamicTo<CSSStyleRule>(parent)) {
      nesting_type = CSSNestingType::kNesting;
      parent_rule_for_nesting = style_rule->GetStyleRule();
      break;
    }
    if (const auto* scope_rule = DynamicTo<CSSScopeRule>(parent)) {
      nesting_type = CSSNestingType::kScope;
      parent_rule_for_nesting =
          scope_rule->GetStyleRuleScope().GetStyleScope().RuleForNesting();
      break;
    }
  }

  // Replace the inner StyleRuleScope with a new rule using the specified
  // prelude.

  CSSStyleSheet* style_sheet = parentStyleSheet();
  StyleSheetContents* contents =
      style_sheet ? style_sheet->Contents() : nullptr;
  auto* parser_context =
      MakeGarbageCollected<CSSParserContext>(*execution_context);
  CSSParserTokenStream stream(value);
  stream.ConsumeWhitespace();
  StyleScope* new_style_scope = StyleScope::Consume(
      stream, parser_context, nesting_type, parent_rule_for_nesting, contents);
  if (!stream.AtEnd()) {
    // Quietly no-op if the provided prelude doesn't parse (similar to the
    // behavior of CSSStyleRule::setSelectorText).
    return;
  }
  if (!new_style_scope) {
    new_style_scope = StyleScope::CreateImplicit();
  }
  // Any '&' selectors in child rules must now point to new_style_scope's
  // internally-held style rule.
  HeapVector<Member<StyleRuleBase>> new_child_rules(
      GetStyleRuleScope().ChildRules(),
      [new_style_scope](StyleRuleBase* child_rule) {
        return child_rule->Clone(new_style_scope->RuleForNesting(),
                                 /*mixin_parameter_bindings=*/nullptr);
      });
  StyleRuleScope* new_group_rule = MakeGarbageCollected<StyleRuleScope>(
      *new_style_scope, std::move(new_child_rules));

  ReplaceChildRuleInParentIfExists(
      /*old_rule=*/group_rule_, new_group_rule, /*position_hint=*/0);

  if (contents) {
    contents->NotifyDiffUnrepresentable();
  }

  Reattach(new_group_rule);
}

StyleRuleScope& CSSScopeRule::GetStyleRuleScope() {
  return *To<StyleRuleScope>(group_rule_.Get());
}

const StyleRuleScope& CSSScopeRule::GetStyleRuleScope() const {
  return *To<StyleRuleScope>(group_rule_.Get());
}

}  // namespace blink
