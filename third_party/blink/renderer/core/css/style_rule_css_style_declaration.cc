/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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

#include "third_party/blink/renderer/core/css/style_rule_css_style_declaration.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/node.h"

namespace blink {

StyleRuleCSSStyleDeclaration::StyleRuleCSSStyleDeclaration(
    MutableCSSPropertyValueSet& property_set_arg,
    CSSRule* parent_rule)
    : PropertySetCSSStyleDeclaration(
          const_cast<Document*>(CSSStyleSheet::SingleOwnerDocument(
              parent_rule->parentStyleSheet()))
              ? const_cast<Document*>(CSSStyleSheet::SingleOwnerDocument(
                                          parent_rule->parentStyleSheet()))
                    ->GetExecutionContext()
              : nullptr,
          property_set_arg),
      parent_rule_(parent_rule) {}

StyleRuleCSSStyleDeclaration::~StyleRuleCSSStyleDeclaration() = default;

void StyleRuleCSSStyleDeclaration::WillMutate() {
  if (parent_rule_ && parent_rule_->parentStyleSheet()) {
    parent_rule_->parentStyleSheet()->WillMutateRules();
  }
}

void StyleRuleCSSStyleDeclaration::DidMutate(MutationType type) {
  // Style sheet mutation needs to be signaled even if the change failed.
  // WillMutate/DidMutate must pair.
  if (parent_rule_ && parent_rule_->parentStyleSheet()) {
    StyleSheetContents* parent_contents =
        parent_rule_->parentStyleSheet()->Contents();
    if (parent_rule_->GetType() == CSSRule::kStyleRule) {
      parent_contents->NotifyRuleChanged(
          static_cast<CSSStyleRule*>(parent_rule_.Get())->GetStyleRule());
    } else {
      parent_contents->NotifyDiffUnrepresentable();
    }
    parent_rule_->parentStyleSheet()->DidMutate(
        CSSStyleSheet::Mutation::kRules);
  }
}

CSSStyleSheet* StyleRuleCSSStyleDeclaration::ParentStyleSheet() const {
  return parent_rule_ ? parent_rule_->parentStyleSheet() : nullptr;
}

void StyleRuleCSSStyleDeclaration::Reattach(
    MutableCSSPropertyValueSet& property_set) {
  property_set_ = &property_set;
}

void StyleRuleCSSStyleDeclaration::Trace(Visitor* visitor) const {
  visitor->Trace(parent_rule_);
  PropertySetCSSStyleDeclaration::Trace(visitor);
}

}  // namespace blink
