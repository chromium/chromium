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

#include "third_party/blink/renderer/core/css/css_page_rule.h"

#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

CSSPageRule::CSSPageRule(StyleRulePage* page_rule, CSSStyleSheet* parent)
    : CSSGroupingRule(page_rule, parent), page_rule_(page_rule) {}

CSSPageRule::~CSSPageRule() = default;

CSSStyleDeclaration* CSSPageRule::style() const {
  if (!properties_cssom_wrapper_) {
    properties_cssom_wrapper_ =
        MakeGarbageCollected<StyleRuleCSSStyleDeclaration>(
            page_rule_->MutableProperties(), const_cast<CSSPageRule*>(this));
  }
  return properties_cssom_wrapper_.Get();
}

String CSSPageRule::selectorText() const {
  StringBuilder text;
  const CSSSelector* selector = page_rule_->Selector();
  if (selector) {
    String page_specification = selector->SelectorText();
    if (!page_specification.empty()) {
      text.Append(page_specification);
    }
  }
  return text.ReleaseString();
}

void CSSPageRule::setSelectorText(const ExecutionContext* execution_context,
                                  const String& selector_text) {
  auto* context = MakeGarbageCollected<CSSParserContext>(
      ParserContext(execution_context->GetSecureContextMode()));
  DCHECK(context);
  CSSSelectorList* selector_list = CSSParser::ParsePageSelector(
      *context, parentStyleSheet() ? parentStyleSheet()->Contents() : nullptr,
      selector_text);
  if (!selector_list || !selector_list->IsValid()) {
    return;
  }

  CSSStyleSheet::RuleMutationScope mutation_scope(this);

  page_rule_->WrapperAdoptSelectorList(selector_list);
}

String CSSPageRule::cssText() const {
  // TODO(mstensho): Serialization needs to be specced:
  // https://github.com/w3c/csswg-drafts/issues/9953
  StringBuilder result;
  result.Append("@page ");
  String page_selectors = selectorText();
  result.Append(page_selectors);
  if (!page_selectors.empty()) {
    result.Append(' ');
  }
  result.Append("{ ");
  String decls = page_rule_->Properties().AsText();
  result.Append(decls);
  if (!decls.empty()) {
    result.Append(' ');
  }

  unsigned size = length();
  for (unsigned i = 0; i < size; i++) {
    result.Append(ItemInternal(i)->cssText());
    result.Append(" ");
  }

  result.Append('}');
  return result.ReleaseString();
}

void CSSPageRule::Reattach(StyleRuleBase* rule) {
  DCHECK(rule);
  page_rule_ = To<StyleRulePage>(rule);
  if (properties_cssom_wrapper_) {
    properties_cssom_wrapper_->Reattach(page_rule_->MutableProperties());
  }
  CSSGroupingRule::Reattach(rule);
}

void CSSPageRule::Trace(Visitor* visitor) const {
  visitor->Trace(page_rule_);
  visitor->Trace(properties_cssom_wrapper_);
  CSSGroupingRule::Trace(visitor);
}

}  // namespace blink
