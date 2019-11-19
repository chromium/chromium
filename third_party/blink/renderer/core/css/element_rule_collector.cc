/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/css/element_rule_collector.h"

#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_supports_rule.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/resolver/style_rule_usage_tracker.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

ElementRuleCollector::ElementRuleCollector(const ElementResolveContext& context,
                                           const SelectorFilter& filter,
                                           ComputedStyle* style)
    : context_(context),
      selector_filter_(filter),
      style_(style),
      pseudo_style_request_(kPseudoIdNone),
      mode_(SelectorChecker::kResolvingStyle),
      can_use_fast_reject_(
          selector_filter_.ParentStackIsConsistent(context.ParentNode())),
      same_origin_only_(false),
      matching_ua_rules_(false),
      include_empty_rules_(false) {}

ElementRuleCollector::~ElementRuleCollector() = default;

const MatchResult& ElementRuleCollector::MatchedResult() const {
  return result_;
}

StyleRuleList* ElementRuleCollector::MatchedStyleRuleList() {
  DCHECK_EQ(mode_, SelectorChecker::kCollectingStyleRules);
  return style_rule_list_.Release();
}

RuleIndexList* ElementRuleCollector::MatchedCSSRuleList() {
  DCHECK_EQ(mode_, SelectorChecker::kCollectingCSSRules);
  return css_rule_list_.Release();
}

void ElementRuleCollector::ClearMatchedRules() {
  matched_rules_.clear();
}

inline StyleRuleList* ElementRuleCollector::EnsureStyleRuleList() {
  if (!style_rule_list_)
    style_rule_list_ = MakeGarbageCollected<StyleRuleList>();
  return style_rule_list_;
}

inline RuleIndexList* ElementRuleCollector::EnsureRuleList() {
  if (!css_rule_list_)
    css_rule_list_ = MakeGarbageCollected<RuleIndexList>();
  return css_rule_list_.Get();
}

void ElementRuleCollector::AddElementStyleProperties(
    const CSSPropertyValueSet* property_set,
    bool is_cacheable) {
  if (!property_set)
    return;
  result_.AddMatchedProperties(property_set);
  if (!is_cacheable)
    result_.SetIsCacheable(false);
}

static bool RulesApplicableInCurrentTreeScope(
    const Element* element,
    const ContainerNode* scoping_node) {
  // Check if the rules come from a shadow style sheet in the same tree scope.
  return !scoping_node ||
         element->ContainingTreeScope() == scoping_node->ContainingTreeScope();
}

template <typename RuleDataListType>
void ElementRuleCollector::CollectMatchingRulesForList(
    const RuleDataListType* rules,
    ShadowV0CascadeOrder cascade_order,
    const MatchRequest& match_request,
    PartNames* part_names) {
  if (!rules)
    return;

  SelectorChecker::Init init;
  init.mode = mode_;
  init.is_ua_rule = matching_ua_rules_;
  init.element_style = style_.get();
  init.scrollbar = pseudo_style_request_.scrollbar;
  init.scrollbar_part = pseudo_style_request_.scrollbar_part;
  init.part_names = part_names;
  SelectorChecker checker(init);
  SelectorChecker::SelectorCheckingContext context(
      &context_.GetElement(), SelectorChecker::kVisitedMatchEnabled);
  context.scope = match_request.scope;
  context.pseudo_id = pseudo_style_request_.pseudo_id;
  context.is_from_vtt = match_request.is_from_vtt;

  unsigned rejected = 0;
  unsigned fast_rejected = 0;
  unsigned matched = 0;

  for (const auto& rule_data : *rules) {
    if (can_use_fast_reject_ &&
        selector_filter_.FastRejectSelector<RuleData::kMaximumIdentifierCount>(
            rule_data->DescendantSelectorIdentifierHashes())) {
      fast_rejected++;
      continue;
    }

    // Don't return cross-origin rules if we did not explicitly ask for them
    // through SetSameOriginOnly.
    if (same_origin_only_ && !rule_data->HasDocumentSecurityOrigin())
      continue;

    StyleRule* rule = rule_data->Rule();

    // If the rule has no properties to apply, then ignore it in the non-debug
    // mode.
    if (!rule->ShouldConsiderForMatchingRules(include_empty_rules_))
      continue;

    SelectorChecker::MatchResult result;
    context.selector = &rule_data->Selector();
    if (!checker.Match(context, result)) {
      rejected++;
      continue;
    }
    if (pseudo_style_request_.pseudo_id != kPseudoIdNone &&
        pseudo_style_request_.pseudo_id != result.dynamic_pseudo) {
      rejected++;
      continue;
    }

    matched++;
    DidMatchRule(rule_data, result, cascade_order, match_request);
  }

  StyleEngine& style_engine =
      context_.GetElement().GetDocument().GetStyleEngine();
  if (!style_engine.Stats())
    return;

  INCREMENT_STYLE_STATS_COUNTER(style_engine, rules_rejected, rejected);
  INCREMENT_STYLE_STATS_COUNTER(style_engine, rules_fast_rejected,
                                fast_rejected);
  INCREMENT_STYLE_STATS_COUNTER(style_engine, rules_matched, matched);
}

DISABLE_CFI_PERF
void ElementRuleCollector::CollectMatchingRules(
    const MatchRequest& match_request,
    ShadowV0CascadeOrder cascade_order,
    bool matching_tree_boundary_rules) {
  DCHECK(match_request.rule_set);

  Element& element = context_.GetElement();
  const AtomicString& pseudo_id = element.ShadowPseudoId();
  if (!pseudo_id.IsEmpty()) {
    DCHECK(element.IsStyledElement());
    CollectMatchingRulesForList(
        match_request.rule_set->ShadowPseudoElementRules(pseudo_id),
        cascade_order, match_request);
  }

  if (element.IsVTTElement())
    CollectMatchingRulesForList(match_request.rule_set->CuePseudoRules(),
                                cascade_order, match_request);
  // Check whether other types of rules are applicable in the current tree
  // scope. Criteria for this:
  // a) the rules are UA rules.
  // b) matching tree boundary crossing rules.
  // c) the rules come from a shadow style sheet in the same tree scope as the
  //    given element.
  // c) is checked in rulesApplicableInCurrentTreeScope.
  if (!matching_ua_rules_ && !matching_tree_boundary_rules &&
      !RulesApplicableInCurrentTreeScope(&element, match_request.scope))
    return;

  // We need to collect the rules for id, class, tag, and everything else into a
  // buffer and then sort the buffer.
  if (element.HasID())
    CollectMatchingRulesForList(
        match_request.rule_set->IdRules(element.IdForStyleResolution()),
        cascade_order, match_request);
  if (element.IsStyledElement() && element.HasClass()) {
    for (wtf_size_t i = 0; i < element.ClassNames().size(); ++i)
      CollectMatchingRulesForList(
          match_request.rule_set->ClassRules(element.ClassNames()[i]),
          cascade_order, match_request);
  }

  if (element.IsLink())
    CollectMatchingRulesForList(match_request.rule_set->LinkPseudoClassRules(),
                                cascade_order, match_request);
  if (SelectorChecker::MatchesFocusPseudoClass(element))
    CollectMatchingRulesForList(match_request.rule_set->FocusPseudoClassRules(),
                                cascade_order, match_request);
  if (SelectorChecker::MatchesSpatialNavigationInterestPseudoClass(element)) {
    CollectMatchingRulesForList(
        match_request.rule_set->SpatialNavigationInterestPseudoClassRules(),
        cascade_order, match_request);
  }
  AtomicString element_name = matching_ua_rules_
                                  ? element.localName()
                                  : element.LocalNameForSelectorMatching();
  CollectMatchingRulesForList(match_request.rule_set->TagRules(element_name),
                              cascade_order, match_request);
  CollectMatchingRulesForList(match_request.rule_set->UniversalRules(),
                              cascade_order, match_request);
}

void ElementRuleCollector::CollectMatchingShadowHostRules(
    const MatchRequest& match_request,
    ShadowV0CascadeOrder cascade_order) {
  CollectMatchingRulesForList(match_request.rule_set->ShadowHostRules(),
                              cascade_order, match_request);
}

void ElementRuleCollector::CollectMatchingPartPseudoRules(
    const MatchRequest& match_request,
    PartNames& part_names,
    ShadowV0CascadeOrder cascade_order) {
  CollectMatchingRulesForList(match_request.rule_set->PartPseudoRules(),
                              cascade_order, match_request, &part_names);
}

template <class CSSRuleCollection>
CSSRule* ElementRuleCollector::FindStyleRule(CSSRuleCollection* css_rules,
                                             StyleRule* style_rule) {
  if (!css_rules)
    return nullptr;
  CSSRule* result = nullptr;
  for (unsigned i = 0; i < css_rules->length() && !result; ++i) {
    CSSRule* css_rule = css_rules->item(i);
    if (auto* css_style_rule = DynamicTo<CSSStyleRule>(css_rule)) {
      if (css_style_rule->GetStyleRule() == style_rule)
        result = css_rule;
    } else if (auto* css_import_rule = DynamicTo<CSSImportRule>(css_rule)) {
      result = FindStyleRule(css_import_rule->styleSheet(), style_rule);
    } else {
      result = FindStyleRule(css_rule->cssRules(), style_rule);
    }
  }
  return result;
}

void ElementRuleCollector::AppendCSSOMWrapperForRule(
    CSSStyleSheet* parent_style_sheet,
    const RuleData* rule_data) {
  // |parentStyleSheet| is 0 if and only if the |rule| is coming from User
  // Agent. In this case, it is safe to create CSSOM wrappers without
  // parentStyleSheets as they will be used only by inspector which will not try
  // to edit them.
  CSSRule* css_rule = nullptr;
  StyleRule* rule = rule_data->Rule();
  if (parent_style_sheet)
    css_rule = FindStyleRule(parent_style_sheet, rule);
  else
    css_rule = rule->CreateCSSOMWrapper();
  DCHECK(!parent_style_sheet || css_rule);
  EnsureRuleList()->emplace_back(css_rule, rule_data->SelectorIndex());
}

void ElementRuleCollector::SortAndTransferMatchedRules() {
  if (matched_rules_.IsEmpty())
    return;

  SortMatchedRules();

  if (mode_ == SelectorChecker::kCollectingStyleRules) {
    for (unsigned i = 0; i < matched_rules_.size(); ++i)
      EnsureStyleRuleList()->push_back(matched_rules_[i].GetRuleData()->Rule());
    return;
  }

  if (mode_ == SelectorChecker::kCollectingCSSRules) {
    for (unsigned i = 0; i < matched_rules_.size(); ++i) {
      AppendCSSOMWrapperForRule(
          const_cast<CSSStyleSheet*>(matched_rules_[i].ParentStyleSheet()),
          matched_rules_[i].GetRuleData());
    }
    return;
  }

  // Now transfer the set of matched rules over to our list of declarations.
  for (unsigned i = 0; i < matched_rules_.size(); i++) {
    const RuleData* rule_data = matched_rules_[i].GetRuleData();
    result_.AddMatchedProperties(
        &rule_data->Rule()->Properties(), rule_data->LinkMatchType(),
        rule_data->GetValidPropertyFilter(matching_ua_rules_));
  }
}

void ElementRuleCollector::DidMatchRule(
    const RuleData* rule_data,
    const SelectorChecker::MatchResult& result,
    ShadowV0CascadeOrder cascade_order,
    const MatchRequest& match_request) {
  PseudoId dynamic_pseudo = result.dynamic_pseudo;
  // If we're matching normal rules, set a pseudo bit if we really just matched
  // a pseudo-element.
  if (dynamic_pseudo != kPseudoIdNone &&
      pseudo_style_request_.pseudo_id == kPseudoIdNone) {
    if (mode_ == SelectorChecker::kCollectingCSSRules ||
        mode_ == SelectorChecker::kCollectingStyleRules)
      return;
    // FIXME: Matching should not modify the style directly.
    if (!style_ || dynamic_pseudo >= kFirstInternalPseudoId)
      return;
    if ((dynamic_pseudo == kPseudoIdBefore ||
         dynamic_pseudo == kPseudoIdAfter) &&
        !rule_data->Rule()->Properties().HasProperty(CSSPropertyID::kContent))
      return;
    if (!rule_data->Rule()->Properties().IsEmpty())
      style_->SetHasPseudoElementStyle(dynamic_pseudo);
  } else {
    matched_rules_.push_back(MatchedRule(
        rule_data, result.specificity, cascade_order,
        match_request.style_sheet_index, match_request.style_sheet));
  }
}

static inline bool CompareRules(const MatchedRule& matched_rule1,
                                const MatchedRule& matched_rule2) {
  unsigned specificity1 = matched_rule1.Specificity();
  unsigned specificity2 = matched_rule2.Specificity();
  if (specificity1 != specificity2)
    return specificity1 < specificity2;

  return matched_rule1.GetPosition() < matched_rule2.GetPosition();
}

void ElementRuleCollector::SortMatchedRules() {
  std::sort(matched_rules_.begin(), matched_rules_.end(), CompareRules);
}

void ElementRuleCollector::AddMatchedRulesToTracker(
    StyleRuleUsageTracker* tracker) const {
  for (auto matched_rule : matched_rules_) {
    tracker->Track(matched_rule.ParentStyleSheet(),
                   matched_rule.GetRuleData()->Rule());
  }
}

}  // namespace blink
