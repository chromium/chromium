/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2002-2003 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002, 2006, 2008, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GROUPING_RULE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GROUPING_RULE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_rule.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExceptionState;
class CSSRuleList;

StyleRule* FindClosestParentStyleRuleOrNull(CSSRule* parent);

// Utility function also used by CSSStyleRule, which can have child rules
// just like CSSGroupingRule can (we share insertRule() / deleteRule()
// implementation). Returns nullptr if an exception was raised.
StyleRuleBase* ParseRuleForInsert(const ExecutionContext* execution_context,
                                  const String& rule_string,
                                  unsigned index,
                                  size_t num_child_rules,
                                  const CSSRule& parent_rule,
                                  ExceptionState& exception_state);

class CORE_EXPORT CSSGroupingRule : public CSSRule {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~CSSGroupingRule() override;

  void Reattach(StyleRuleBase*) override;

  CSSRuleList* cssRules() const override;

  unsigned insertRule(const ExecutionContext*,
                      const String& rule,
                      unsigned index,
                      ExceptionState&);
  void deleteRule(unsigned index, ExceptionState&);

  // For CSSRuleList
  unsigned length() const;
  CSSRule* Item(unsigned index, bool trigger_use_counters = true) const;

  // Get an item, but signal that it's been requested internally from the
  // engine, and not directly from a script.
  CSSRule* ItemInternal(unsigned index) const {
    return Item(index, /*trigger_use_counters=*/false);
  }

  void Trace(Visitor*) const override;

 protected:
  CSSGroupingRule(StyleRuleGroup* group_rule, CSSStyleSheet* parent);

  void AppendCSSTextForItems(StringBuilder&) const;

  Member<StyleRuleGroup> group_rule_;
  mutable HeapVector<Member<CSSRule>> child_rule_cssom_wrappers_;
  mutable Member<CSSRuleList> rule_list_cssom_wrapper_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSS_GROUPING_RULE_H_
