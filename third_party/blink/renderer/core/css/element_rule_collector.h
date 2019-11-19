/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 * All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ELEMENT_RULE_COLLECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ELEMENT_RULE_COLLECTOR_H_

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/pseudo_style_request.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/resolver/match_request.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CSSStyleSheet;
class PartNames;
class RuleData;
class SelectorFilter;
class StyleRuleUsageTracker;

// TODO(kochi): ShadowV0CascadeOrder is used only for Shadow DOM V0
// bug-compatible cascading order. Once Shadow DOM V0 implementation is gone,
// remove this completely.
using ShadowV0CascadeOrder = unsigned;
const ShadowV0CascadeOrder kIgnoreCascadeOrder = 0;

class MatchedRule {
  DISALLOW_NEW();

 public:
  MatchedRule(const RuleData* rule_data,
              unsigned specificity,
              ShadowV0CascadeOrder cascade_order,
              unsigned style_sheet_index,
              const CSSStyleSheet* parent_style_sheet)
      : rule_data_(rule_data),
        specificity_(specificity),
        parent_style_sheet_(parent_style_sheet) {
    DCHECK(rule_data_);
    static const unsigned kBitsForPositionInRuleData = 18;
    static const unsigned kBitsForStyleSheetIndex = 32;
    position_ = ((uint64_t)cascade_order
                 << (kBitsForStyleSheetIndex + kBitsForPositionInRuleData)) +
                ((uint64_t)style_sheet_index << kBitsForPositionInRuleData) +
                rule_data_->GetPosition();
  }

  const RuleData* GetRuleData() const { return rule_data_; }
  uint64_t GetPosition() const { return position_; }
  unsigned Specificity() const {
    return GetRuleData()->Specificity() + specificity_;
  }
  const CSSStyleSheet* ParentStyleSheet() const { return parent_style_sheet_; }
  void Trace(blink::Visitor* visitor) {
    visitor->Trace(parent_style_sheet_);
    visitor->Trace(rule_data_);
  }

 private:
  Member<const RuleData> rule_data_;
  unsigned specificity_;
  uint64_t position_;
  Member<const CSSStyleSheet> parent_style_sheet_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MatchedRule)

namespace blink {

using StyleRuleList = HeapVector<Member<StyleRule>>;

// Manages the process of finding what rules in a RuleSet apply to a given
// Element. These tend to be used several times in different contexts and should
// have ClearMatchedRules called before use.
//
// ElementRuleCollector is designed to be used as a stack object.
// Create one, ask what rules the ElementResolveContext matches
// and then let it go out of scope.
// FIXME: Currently it modifies the ComputedStyle but should not!
class ElementRuleCollector {
  STACK_ALLOCATED();

 public:
  ElementRuleCollector(const ElementResolveContext&,
                       const SelectorFilter&,
                       ComputedStyle* = nullptr);
  ~ElementRuleCollector();

  void SetMode(SelectorChecker::Mode mode) { mode_ = mode; }
  void SetPseudoElementStyleRequest(const PseudoElementStyleRequest& request) {
    pseudo_style_request_ = request;
  }
  void SetSameOriginOnly(bool f) { same_origin_only_ = f; }

  void SetMatchingUARules(bool matching_ua_rules) {
    matching_ua_rules_ = matching_ua_rules;
  }

  const MatchResult& MatchedResult() const;
  StyleRuleList* MatchedStyleRuleList();
  RuleIndexList* MatchedCSSRuleList();

  void CollectMatchingRules(const MatchRequest&,
                            ShadowV0CascadeOrder = kIgnoreCascadeOrder,
                            bool matching_tree_boundary_rules = false);
  void CollectMatchingShadowHostRules(
      const MatchRequest&,
      ShadowV0CascadeOrder = kIgnoreCascadeOrder);
  void CollectMatchingPartPseudoRules(
      const MatchRequest&,
      PartNames&,
      ShadowV0CascadeOrder = kIgnoreCascadeOrder);
  void SortAndTransferMatchedRules();
  void ClearMatchedRules();
  void AddElementStyleProperties(const CSSPropertyValueSet*,
                                 bool is_cacheable = true);
  void FinishAddingUARules() { result_.FinishAddingUARules(); }
  void FinishAddingUserRules() {
    result_.FinishAddingUserRules();
  }
  void FinishAddingAuthorRulesForTreeScope() {
    result_.FinishAddingAuthorRulesForTreeScope();
  }
  void SetIncludeEmptyRules(bool include) { include_empty_rules_ = include; }
  bool IncludeEmptyRules() const { return include_empty_rules_; }
  bool IsCollectingForPseudoElement() const {
    return pseudo_style_request_.pseudo_id != kPseudoIdNone;
  }

  void AddMatchedRulesToTracker(StyleRuleUsageTracker*) const;

 private:
  template <typename RuleDataListType>
  void CollectMatchingRulesForList(const RuleDataListType*,
                                   ShadowV0CascadeOrder,
                                   const MatchRequest&,
                                   PartNames* = nullptr);

  void DidMatchRule(const RuleData*,
                    const SelectorChecker::MatchResult&,
                    ShadowV0CascadeOrder,
                    const MatchRequest&);

  template <class CSSRuleCollection>
  CSSRule* FindStyleRule(CSSRuleCollection*, StyleRule*);
  void AppendCSSOMWrapperForRule(CSSStyleSheet*, const RuleData*);

  void SortMatchedRules();

  RuleIndexList* EnsureRuleList();
  StyleRuleList* EnsureStyleRuleList();

 private:
  const ElementResolveContext& context_;
  const SelectorFilter& selector_filter_;
  scoped_refptr<ComputedStyle>
      style_;  // FIXME: This can be mutated during matching!

  PseudoElementStyleRequest pseudo_style_request_;
  SelectorChecker::Mode mode_;
  bool can_use_fast_reject_;
  bool same_origin_only_;
  bool matching_ua_rules_;
  bool include_empty_rules_;

  HeapVector<MatchedRule, 32> matched_rules_;

  // Output.
  Member<RuleIndexList> css_rule_list_;
  Member<StyleRuleList> style_rule_list_;
  MatchResult result_;
  DISALLOW_COPY_AND_ASSIGN(ElementRuleCollector);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_ELEMENT_RULE_COLLECTOR_H_
