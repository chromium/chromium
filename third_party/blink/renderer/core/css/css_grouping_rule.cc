/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/css/css_grouping_rule.h"

#include "third_party/blink/renderer/core/css/css_page_rule.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StyleRule* FindClosestParentStyleRuleOrNull(CSSRule* parent) {
  if (parent == nullptr) {
    return nullptr;
  }
  if (parent->type() == CSSRule::kStyleRule) {
    return To<CSSStyleRule>(parent)->GetStyleRule();
  }
  return FindClosestParentStyleRuleOrNull(parent->parentRule());
}

StyleRuleBase* ParseRuleForInsert(const ExecutionContext* execution_context,
                                  const String& rule_string,
                                  unsigned index,
                                  size_t num_child_rules,
                                  CSSRule& parent_rule,
                                  ExceptionState& exception_state) {
  if (index > num_child_rules) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "the index " + String::Number(index) +
            " must be less than or equal to the length of the rule list.");
    return nullptr;
  }

  CSSStyleSheet* style_sheet = parent_rule.parentStyleSheet();
  auto* context = MakeGarbageCollected<CSSParserContext>(
      parent_rule.ParserContext(execution_context->GetSecureContextMode()),
      style_sheet);
  StyleRuleBase* new_rule = nullptr;
  if (IsA<CSSPageRule>(parent_rule)) {
    new_rule = CSSParser::ParseMarginRule(
        context, style_sheet ? style_sheet->Contents() : nullptr, rule_string);
  } else {
    StyleRule* parent_rule_for_nesting =
        FindClosestParentStyleRuleOrNull(&parent_rule);
    CSSNestingType nesting_type = parent_rule_for_nesting
                                      ? CSSNestingType::kNesting
                                      : CSSNestingType::kNone;
    new_rule = CSSParser::ParseRule(
        context, style_sheet ? style_sheet->Contents() : nullptr, nesting_type,
        parent_rule_for_nesting, rule_string);

    if (!new_rule && parent_rule_for_nesting &&
        RuntimeEnabledFeatures::CSSNestedDeclarationsEnabled()) {
      // Retry as a CSSNestedDeclarations rule.
      // https://drafts.csswg.org/cssom/#insert-a-css-rule
      new_rule = CSSParser::ParseNestedDeclarationsRule(
          context, nesting_type, parent_rule_for_nesting, rule_string);
    }
  }

  if (!new_rule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        "the rule '" + rule_string + "' is invalid and cannot be parsed.");
    return nullptr;
  }

  if (new_rule->IsNamespaceRule()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "'@namespace' rules cannot be inserted inside a group rule.");
    return nullptr;
  }

  if (new_rule->IsImportRule()) {
    // FIXME: an HierarchyRequestError should also be thrown for a nested @media
    // rule. They are currently not getting parsed, resulting in a SyntaxError
    // to get raised above.
    exception_state.ThrowDOMException(
        DOMExceptionCode::kHierarchyRequestError,
        "'@import' rules cannot be inserted inside a group rule.");
    return nullptr;
  }

  if (!new_rule->IsConditionRule() && !new_rule->IsStyleRule() &&
      !new_rule->IsNestedDeclarationsRule()) {
    for (const CSSRule* current = &parent_rule; current != nullptr;
         current = current->parentRule()) {
      if (IsA<CSSStyleRule>(current)) {
        // We are in nesting context (directly or indirectly),
        // so inserting this rule is not allowed.
        exception_state.ThrowDOMException(
            DOMExceptionCode::kHierarchyRequestError,
            "Only conditional nested group rules, style rules, and nested "
            "declaration rules may be nested.");
        return nullptr;
      }
    }
  }

  return new_rule;
}

CSSGroupingRule::CSSGroupingRule(StyleRuleGroup* group_rule,
                                 CSSStyleSheet* parent)
    : CSSRule(parent),
      group_rule_(group_rule),
      child_rule_cssom_wrappers_(group_rule->ChildRules().size()) {}

CSSGroupingRule::~CSSGroupingRule() = default;

unsigned CSSGroupingRule::insertRule(const ExecutionContext* execution_context,
                                     const String& rule_string,
                                     unsigned index,
                                     ExceptionState& exception_state) {
  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            group_rule_->ChildRules().size());

  StyleRuleBase* new_rule = ParseRuleForInsert(
      execution_context, rule_string, index, group_rule_->ChildRules().size(),
      *this, exception_state);

  if (new_rule == nullptr) {
    // Already raised an exception above.
    return 0;
  } else {
    CSSStyleSheet::RuleMutationScope mutation_scope(this);
    group_rule_->WrapperInsertRule(parentStyleSheet(), index, new_rule);
    child_rule_cssom_wrappers_.insert(index, Member<CSSRule>(nullptr));
    return index;
  }
}

void CSSGroupingRule::deleteRule(unsigned index,
                                 ExceptionState& exception_state) {
  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            group_rule_->ChildRules().size());

  if (index >= group_rule_->ChildRules().size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "the index " + String::Number(index) +
            " is greated than the length of the rule list.");
    return;
  }

  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  group_rule_->WrapperRemoveRule(parentStyleSheet(), index);

  if (child_rule_cssom_wrappers_[index]) {
    child_rule_cssom_wrappers_[index]->SetParentRule(nullptr);
  }
  child_rule_cssom_wrappers_.EraseAt(index);
}

void CSSGroupingRule::AppendCSSTextForItems(StringBuilder& result) const {
  // https://drafts.csswg.org/cssom-1/#serialize-a-css-rule,
  // using CSSMediaRule as an example:

  // The result of concatenating the following:
  // 1. The string "@media", followed by a single SPACE (U+0020).
  // 2. The result of performing serialize a media query list on rule’s media
  //    query list.
  // [1–2 is done in the parent, and is different for @container etc.]

  // 3. A single SPACE (U+0020), followed by the string "{", i.e., LEFT CURLY
  //    BRACKET (U+007B), followed by a newline.
  result.Append(" {\n");

  // 4. The result of performing serialize a CSS rule on each rule in the rule’s
  //    cssRules list, filtering out empty strings, indenting each item
  //    with two spaces, all joined with newline.
  for (unsigned i = 0; i < length(); ++i) {
    CSSRule* child = ItemInternal(i);
    String child_text = child->cssText();
    if (!child_text.empty()) {
      result.Append("  ");
      result.Append(child_text);
      result.Append('\n');
    }
  }

  // A newline, followed by the string "}", i.e., RIGHT CURLY BRACKET (U+007D)
  result.Append('}');
}

unsigned CSSGroupingRule::length() const {
  return group_rule_->ChildRules().size();
}

CSSRule* CSSGroupingRule::Item(unsigned index,
                               bool trigger_use_counters) const {
  if (index >= length()) {
    return nullptr;
  }
  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            group_rule_->ChildRules().size());
  Member<CSSRule>& rule = child_rule_cssom_wrappers_[index];
  if (!rule) {
    rule = group_rule_->ChildRules()[index]->CreateCSSOMWrapper(
        index, const_cast<CSSGroupingRule*>(this), trigger_use_counters);
  }
  return rule.Get();
}

CSSRuleList* CSSGroupingRule::cssRules() const {
  if (!rule_list_cssom_wrapper_) {
    rule_list_cssom_wrapper_ =
        MakeGarbageCollected<LiveCSSRuleList<CSSGroupingRule>>(
            const_cast<CSSGroupingRule*>(this));
  }
  return rule_list_cssom_wrapper_.Get();
}

void CSSGroupingRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  group_rule_ = static_cast<StyleRuleGroup*>(rule);
  for (unsigned i = 0; i < child_rule_cssom_wrappers_.size(); ++i) {
    if (child_rule_cssom_wrappers_[i]) {
      child_rule_cssom_wrappers_[i]->Reattach(
          group_rule_->ChildRules()[i].Get());
    }
  }
}

void CSSGroupingRule::Trace(Visitor* visitor) const {
  CSSRule::Trace(visitor);
  visitor->Trace(child_rule_cssom_wrappers_);
  visitor->Trace(group_rule_);
  visitor->Trace(rule_list_cssom_wrapper_);
}

}  // namespace blink
