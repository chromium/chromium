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
#include "third_party/blink/renderer/core/css/css_scope_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/strcat.h"
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

const CSSRule* FindClosestStyleOrScopeRule(const CSSRule* parent) {
  if (parent == nullptr) {
    return nullptr;
  }
  if (IsA<CSSStyleRule>(parent) || IsA<CSSScopeRule>(parent)) {
    return parent;
  }
  return FindClosestStyleOrScopeRule(parent->parentRule());
}

// Parsing child rules is highly dependent on the ancestor rules.
// Under normal, full-stylesheet parsing, this information is available
// on the stack, but for rule insertion we need to traverse and inspect
// the ancestor chain.
//
// [1] https://drafts.csswg.org/css-nesting-1/#nested-group-rules
NestingContext CalculateNestingContext(const CSSRule* parent_rule) {
  if (const CSSRule* closest_style_or_scope_rule =
          FindClosestStyleOrScopeRule(parent_rule)) {
    if (const auto* style_rule =
            DynamicTo<CSSStyleRule>(closest_style_or_scope_rule)) {
      return {.nesting_type = CSSNestingType::kNesting,
              .parent_rule_for_nesting = style_rule->GetStyleRule()};
    } else if (const auto* scope_rule =
                   DynamicTo<CSSScopeRule>(closest_style_or_scope_rule)) {
      // The <scope-start> selector acts as the parent style rule.
      // https://drafts.csswg.org/css-nesting-1/#nesting-at-scope
      return {
          .nesting_type = CSSNestingType::kScope,
          .parent_rule_for_nesting =
              scope_rule->GetStyleRuleScope().GetStyleScope().RuleForNesting()};
    } else {
      NOTREACHED();
    }
  }

  return {.nesting_type = CSSNestingType::kNone,
          .parent_rule_for_nesting = nullptr};
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
        StrCat(
            {"the index ", String::Number(index),
             " must be less than or equal to the length of the rule list."}));
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
    NestingContext nesting_context = CalculateNestingContext(&parent_rule);

    new_rule = CSSParser::ParseRule(
        context, style_sheet ? style_sheet->Contents() : nullptr,
        nesting_context.nesting_type, nesting_context.parent_rule_for_nesting,
        rule_string);

    bool allow_nested_declarations =
        nesting_context.nesting_type != CSSNestingType::kNone;
    if (!new_rule && allow_nested_declarations) {
      // Retry as a CSSNestedDeclarations rule.
      // https://drafts.csswg.org/cssom/#insert-a-css-rule
      new_rule = CSSParser::ParseNestedDeclarationsRule(
          context, nesting_context.nesting_type,
          nesting_context.parent_rule_for_nesting, rule_string);
    }
  }

  if (!new_rule) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kSyntaxError,
        StrCat(
            {"the rule '", rule_string, "' is invalid and cannot be parsed."}));
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

  if (!new_rule->IsConditionRule() && !new_rule->IsScopeRule() &&
      !new_rule->IsStyleRule() && !new_rule->IsNestedDeclarationsRule() &&
      !new_rule->IsApplyMixinRule()) {
    for (const CSSRule* current = &parent_rule; current != nullptr;
         current = current->parentRule()) {
      if (IsA<CSSStyleRule>(current)) {
        // We are in nesting context (directly or indirectly),
        // so inserting this rule is not allowed.
        exception_state.ThrowDOMException(
            DOMExceptionCode::kHierarchyRequestError,
            "Only conditional nested group rules, style rules, @scope rules,"
            "@apply rules, and nested declaration rules may be nested.");
        return nullptr;
      }
    }
  }

  return new_rule;
}

template <typename VectorType>
void ParseAndQuietlyInsertRule(
    const ExecutionContext* execution_context,
    const String& rule_string,
    unsigned index,
    CSSRule& parent_rule,
    VectorType& child_rules,
    HeapVector<Member<CSSRule>>& child_rule_cssom_wrappers) {
  CHECK_EQ(child_rule_cssom_wrappers.size(), child_rules.size());
  StyleRuleBase* new_rule =
      ParseRuleForInsert(execution_context, rule_string, index,
                         child_rules.size(), parent_rule, ASSERT_NO_EXCEPTION);
  CHECK(new_rule);
  child_rules.insert(index, new_rule);
  child_rule_cssom_wrappers.insert(index, Member<CSSRule>(nullptr));
}
template void ParseAndQuietlyInsertRule<GCedHeapVector<Member<StyleRuleBase>>>(
    const ExecutionContext* execution_context,
    const String& rule_string,
    unsigned index,
    CSSRule& parent_rule,
    GCedHeapVector<Member<StyleRuleBase>>& child_rules,
    HeapVector<Member<CSSRule>>& child_rule_cssom_wrappers);
template void ParseAndQuietlyInsertRule<HeapVector<Member<StyleRuleBase>>>(
    const ExecutionContext* execution_context,
    const String& rule_string,
    unsigned index,
    CSSRule& parent_rule,
    HeapVector<Member<StyleRuleBase>>& child_rules,
    HeapVector<Member<CSSRule>>& child_rule_cssom_wrappers);

template <typename VectorType>
void QuietlyDeleteRule(unsigned index,
                       VectorType& child_rules,
                       HeapVector<Member<CSSRule>>& child_rule_cssom_wrappers) {
  CHECK_EQ(child_rule_cssom_wrappers.size(), child_rules.size());
  CHECK_LT(index, child_rules.size());
  child_rules.EraseAt(index);
  if (child_rule_cssom_wrappers[index]) {
    child_rule_cssom_wrappers[index]->SetParentRule(nullptr);
  }
  child_rule_cssom_wrappers.EraseAt(index);
}
template void QuietlyDeleteRule<HeapVector<Member<StyleRuleBase>>>(
    unsigned index,
    HeapVector<Member<StyleRuleBase>>& child_rules,
    HeapVector<Member<CSSRule>>& child_rule_cssom_wrappers);
template void QuietlyDeleteRule<GCedHeapVector<Member<StyleRuleBase>>>(
    unsigned index,
    GCedHeapVector<Member<StyleRuleBase>>& child_rules,
    HeapVector<Member<CSSRule>>& child_rule_cssom_wrappers);

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
        StrCat({"the index ", String::Number(index),
                " is greated than the length of the rule list."}));
    return;
  }

  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  group_rule_->WrapperRemoveRule(parentStyleSheet(), index);

  if (child_rule_cssom_wrappers_[index]) {
    child_rule_cssom_wrappers_[index]->SetParentRule(nullptr);
  }
  child_rule_cssom_wrappers_.EraseAt(index);
}

void CSSGroupingRule::QuietlyInsertRule(
    const ExecutionContext* execution_context,
    const String& rule,
    unsigned index) {
  ParseAndQuietlyInsertRule(execution_context, rule, index,
                            /*parent_rule=*/*this, group_rule_->ChildRules(),
                            child_rule_cssom_wrappers_);
}

void CSSGroupingRule::QuietlyDeleteRule(unsigned index) {
  blink::QuietlyDeleteRule(index, group_rule_->ChildRules(),
                           child_rule_cssom_wrappers_);
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
