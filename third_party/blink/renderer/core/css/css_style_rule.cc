/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/css/css_style_rule.h"

#include "third_party/blink/renderer/core/css/css_grouping_rule.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/cssom/declared_style_property_map.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

using SelectorTextCache = HeapHashMap<WeakMember<const CSSStyleRule>, String>;

static SelectorTextCache& GetSelectorTextCache() {
  DEFINE_STATIC_LOCAL(Persistent<SelectorTextCache>, cache,
                      (MakeGarbageCollected<SelectorTextCache>()));
  return *cache;
}

CSSStyleRule::CSSStyleRule(StyleRule* style_rule,
                           CSSStyleSheet* parent,
                           wtf_size_t position_hint)
    : CSSRule(parent),
      style_rule_(style_rule),
      style_map_(MakeGarbageCollected<DeclaredStylePropertyMap>(this)),
      position_hint_(position_hint),
      child_rule_cssom_wrappers_(
          style_rule->ChildRules() ? style_rule->ChildRules()->size() : 0) {}

CSSStyleRule::~CSSStyleRule() = default;

CSSStyleDeclaration* CSSStyleRule::style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ =
        MakeGarbageCollected<StyleRuleCSSStyleDeclaration>(
            style_rule_->MutableProperties(), const_cast<CSSStyleRule*>(this));
  }
  return properties_cssom_wrapper_.Get();
}

String CSSStyleRule::selectorText() const {
  if (HasCachedSelectorText()) {
    DCHECK(GetSelectorTextCache().Contains(this));
    return GetSelectorTextCache().at(this);
  }

  DCHECK(!GetSelectorTextCache().Contains(this));
  String text = style_rule_->SelectorsText();
  GetSelectorTextCache().Set(this, text);
  SetHasCachedSelectorText(true);
  return text;
}

void CSSStyleRule::setSelectorText(const ExecutionContext* execution_context,
                                   const String& selector_text) {
  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  const auto* context = MakeGarbageCollected<CSSParserContext>(
      ParserContext(execution_context->GetSecureContextMode()));
  StyleSheetContents* parent_contents =
      parentStyleSheet() ? parentStyleSheet()->Contents() : nullptr;
  HeapVector<CSSSelector> arena;
  StyleRule* parent_rule_for_nesting =
      FindClosestParentStyleRuleOrNull(parentRule());
  CSSNestingType nesting_type = parent_rule_for_nesting
                                    ? CSSNestingType::kNesting
                                    : CSSNestingType::kNone;
  base::span<CSSSelector> selector_vector = CSSParser::ParseSelector(
      context, nesting_type, parent_rule_for_nesting, /*is_within_scope=*/false,
      parent_contents, selector_text, arena);
  if (selector_vector.empty()) {
    return;
  }

  StyleRule* new_style_rule =
      StyleRule::Create(selector_vector, std::move(*style_rule_));
  if (parent_contents) {
    position_hint_ = parent_contents->ReplaceRuleIfExists(
        style_rule_, new_style_rule, position_hint_);
  }

  // If we have any nested rules, update their parent selector(s) to point to
  // our newly created StyleRule instead of the old one.
  if (new_style_rule->ChildRules()) {
    for (StyleRuleBase* child_rule : *new_style_rule->ChildRules()) {
      child_rule->Reparent(new_style_rule);
    }
  }

  style_rule_ = new_style_rule;

  if (HasCachedSelectorText()) {
    GetSelectorTextCache().erase(this);
    SetHasCachedSelectorText(false);
  }
}

String CSSStyleRule::cssText() const {
  // Referring to https://drafts.csswg.org/cssom-1/#serialize-a-css-rule:

  // Step 1.
  StringBuilder result;
  result.Append(selectorText());
  result.Append(" {");

  // Step 2.
  String decls = style_rule_->Properties().AsText();

  // Step 3.
  StringBuilder rules;
  unsigned size = length();
  for (unsigned i = 0; i < size; ++i) {
    // Step 6.2 for rules.
    String item_text = ItemInternal(i)->cssText();
    if (!item_text.empty()) {
      rules.Append("\n  ");
      rules.Append(item_text);
    }
  }

  // Step 4.
  if (decls.empty() && rules.empty()) {
    result.Append(" }");
    return result.ReleaseString();
  }

  // Step 5.
  if (rules.empty()) {
    result.Append(' ');
    result.Append(decls);
    result.Append(" }");
    return result.ReleaseString();
  }

  // Step 6.
  if (!decls.empty()) {
    // Step 6.2 for decls (we don't do 6.1 explicitly).
    result.Append("\n  ");
    result.Append(decls);
  }

  // Step 6.2 for rules was done above.
  result.Append(rules);

  result.Append("\n}");
  return result.ReleaseString();
}

void CSSStyleRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  style_rule_ = To<StyleRule>(rule);
  if (properties_cssom_wrapper_) {
    properties_cssom_wrapper_->Reattach(style_rule_->MutableProperties());
  }
  for (unsigned i = 0; i < child_rule_cssom_wrappers_.size(); ++i) {
    if (child_rule_cssom_wrappers_[i]) {
      child_rule_cssom_wrappers_[i]->Reattach(
          (*style_rule_->ChildRules())[i].Get());
    }
  }
}

void CSSStyleRule::Trace(Visitor* visitor) const {
  visitor->Trace(style_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  visitor->Trace(style_map_);
  visitor->Trace(child_rule_cssom_wrappers_);
  visitor->Trace(rule_list_cssom_wrapper_);
  CSSRule::Trace(visitor);
}

unsigned CSSStyleRule::length() const {
  if (style_rule_->ChildRules()) {
    return style_rule_->ChildRules()->size();
  } else {
    return 0;
  }
}

CSSRule* CSSStyleRule::Item(unsigned index, bool trigger_use_counters) const {
  if (index >= length()) {
    return nullptr;
  }
  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            style_rule_->ChildRules()->size());
  Member<CSSRule>& rule = child_rule_cssom_wrappers_[index];
  if (!rule) {
    rule = (*style_rule_->ChildRules())[index]->CreateCSSOMWrapper(
        index, const_cast<CSSStyleRule*>(this), trigger_use_counters);
  }
  return rule.Get();
}

CSSRuleList* CSSStyleRule::cssRules() const {
  if (!rule_list_cssom_wrapper_) {
    rule_list_cssom_wrapper_ =
        MakeGarbageCollected<LiveCSSRuleList<CSSStyleRule>>(
            const_cast<CSSStyleRule*>(this));
  }
  return rule_list_cssom_wrapper_.Get();
}

unsigned CSSStyleRule::insertRule(const ExecutionContext* execution_context,
                                  const String& rule_string,
                                  unsigned index,
                                  ExceptionState& exception_state) {
  if (style_rule_->ChildRules() == nullptr) {
    // Implicitly zero rules.
    if (index > 0) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kIndexSizeError,
          "the index " + String::Number(index) +
              " must be less than or equal to the length of the rule list.");
      return 0;
    }
    style_rule_->EnsureChildRules();
  }

  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            style_rule_->ChildRules()->size());

  StyleRuleBase* new_rule = ParseRuleForInsert(
      execution_context, rule_string, index, style_rule_->ChildRules()->size(),
      *this, exception_state);

  if (new_rule == nullptr) {
    // Already raised an exception above.
    return 0;
  } else {
    CSSStyleSheet::RuleMutationScope mutation_scope(this);
    style_rule_->WrapperInsertRule(index, new_rule);
    child_rule_cssom_wrappers_.insert(index, Member<CSSRule>(nullptr));
    return index;
  }
}

void CSSStyleRule::deleteRule(unsigned index, ExceptionState& exception_state) {
  if (style_rule_->ChildRules() == nullptr ||
      index >= style_rule_->ChildRules()->size()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "the index " + String::Number(index) +
            " is greated than the length of the rule list.");
    return;
  }

  DCHECK_EQ(child_rule_cssom_wrappers_.size(),
            style_rule_->ChildRules()->size());

  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  style_rule_->WrapperRemoveRule(index);

  if (child_rule_cssom_wrappers_[index]) {
    child_rule_cssom_wrappers_[index]->SetParentRule(nullptr);
  }
  child_rule_cssom_wrappers_.EraseAt(index);
}

}  // namespace blink
