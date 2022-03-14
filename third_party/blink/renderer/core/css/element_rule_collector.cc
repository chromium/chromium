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

#include "base/containers/span.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_supports_rule.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/resolver/style_rule_usage_tracker.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

unsigned AdjustLinkMatchType(EInsideLink inside_link,
                             unsigned link_match_type) {
  if (inside_link == EInsideLink::kNotInsideLink)
    return CSSSelector::kMatchLink;
  return link_match_type;
}

unsigned LinkMatchTypeFromInsideLink(EInsideLink inside_link) {
  switch (inside_link) {
    case EInsideLink::kNotInsideLink:
      return CSSSelector::kMatchAll;
    case EInsideLink::kInsideVisitedLink:
      return CSSSelector::kMatchVisited;
    case EInsideLink::kInsideUnvisitedLink:
      return CSSSelector::kMatchLink;
  }
}

bool EvaluateAndAddContainerQueries(
    const ContainerQuery& container_query,
    const StyleRecalcContext& style_recalc_context,
    MatchResult& result) {
  for (const ContainerQuery* current = &container_query; current;
       current = current->Parent()) {
    if (!ContainerQueryEvaluator::EvalAndAdd(style_recalc_context, *current,
                                             result)) {
      return false;
    }
  }

  return true;
}

bool AffectsAnimations(const RuleData& rule_data) {
  const CSSPropertyValueSet& properties = rule_data.Rule()->Properties();
  unsigned count = properties.PropertyCount();
  for (unsigned i = 0; i < count; ++i) {
    auto reference = properties.PropertyAt(i);
    CSSPropertyID id = reference.Id();
    if (id == CSSPropertyID::kAll)
      return true;
    if (id == CSSPropertyID::kVariable)
      continue;
    if (CSSProperty::Get(id).IsAnimationProperty())
      return true;
  }
  return false;
}

// Sequentially scans a sorted list of RuleSet::LayerInterval and seeks for the
// cascade layer for a rule (given by its position). SeekLayerOrder() must be
// called with non-decreasing rule positions, so that we only need to go through
// the layer list at most once for all SeekLayerOrder() calls.
class CascadeLayerSeeker {
  STACK_ALLOCATED();

 public:
  explicit CascadeLayerSeeker(const MatchRequest& request)
      : layers_(request.rule_set->LayerIntervals()),
        layer_iter_(layers_.begin()),
        layer_map_(FindLayerMap(request)) {}

  unsigned SeekLayerOrder(unsigned rule_position) {
#if DCHECK_IS_ON()
    DCHECK_GE(rule_position, last_rule_position_);
    last_rule_position_ = rule_position;
#endif

    if (!layer_map_)
      return CascadeLayerMap::kImplicitOuterLayerOrder;

    while (layer_iter_ != layers_.end() &&
           layer_iter_->start_position <= rule_position)
      ++layer_iter_;
    if (layer_iter_ == layers_.begin())
      return CascadeLayerMap::kImplicitOuterLayerOrder;
    return layer_map_->GetLayerOrder(*std::prev(layer_iter_)->layer);
  }

 private:
  static const CascadeLayerMap* FindLayerMap(const MatchRequest& request) {
    // VTT embedded style is not in any layer.
    if (request.vtt_originating_element)
      return nullptr;
    if (request.scope) {
      return request.scope->ContainingTreeScope()
          .GetScopedStyleResolver()
          ->GetCascadeLayerMap();
    }
    // Assume there are no UA cascade layers, so we only check user layers.
    if (!request.style_sheet)
      return nullptr;
    Document* document = request.style_sheet->OwnerDocument();
    if (!document)
      return nullptr;
    return document->GetStyleEngine().GetUserCascadeLayerMap();
  }

  const HeapVector<RuleSet::LayerInterval>& layers_;
  const RuleSet::LayerInterval* layer_iter_;
  const CascadeLayerMap* layer_map_ = nullptr;
#if DCHECK_IS_ON()
  unsigned last_rule_position_ = 0;
#endif
};

}  // namespace

ElementRuleCollector::ElementRuleCollector(
    const ElementResolveContext& context,
    const StyleRecalcContext& style_recalc_context,
    const SelectorFilter& filter,
    MatchResult& result,
    ComputedStyle* style,
    EInsideLink inside_link)
    : context_(context),
      style_recalc_context_(style_recalc_context),
      selector_filter_(filter),
      style_(style),
      mode_(SelectorChecker::kResolvingStyle),
      can_use_fast_reject_(
          selector_filter_.ParentStackIsConsistent(context.ParentNode())),
      same_origin_only_(false),
      matching_ua_rules_(false),
      include_empty_rules_(false),
      inside_link_(inside_link),
      result_(result) {}

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
    bool is_cacheable,
    bool is_inline_style) {
  if (!property_set)
    return;
  auto link_match_type = static_cast<unsigned>(CSSSelector::kMatchAll);
  result_.AddMatchedProperties(
      property_set,
      AddMatchedPropertiesOptions::Builder()
          .SetLinkMatchType(AdjustLinkMatchType(inside_link_, link_match_type))
          .SetIsInlineStyle(is_inline_style)
          .Build());
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
    const MatchRequest& match_request,
    const SelectorChecker& checker,
    PartRequest* part_request) {
  if (!rules)
    return;

  SelectorChecker::SelectorCheckingContext context(&context_.GetElement());
  context.scope = match_request.scope;
  context.pseudo_id = pseudo_style_request_.pseudo_id;
  context.vtt_originating_element = match_request.vtt_originating_element;

  CascadeLayerSeeker layer_seeker(match_request);

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

    const auto& selector = rule_data->Selector();
    if (UNLIKELY(part_request && part_request->for_shadow_pseudo)) {
      if (!selector.IsAllowedAfterPart()) {
        DCHECK_EQ(selector.GetPseudoType(), CSSSelector::kPseudoPart);
        rejected++;
        continue;
      }
      DCHECK_EQ(selector.Relation(), CSSSelector::kUAShadow);
    }

    SelectorChecker::MatchResult result;
    context.selector = &selector;
    context.is_inside_visited_link =
        rule_data->LinkMatchType() == CSSSelector::kMatchVisited;
    DCHECK(!context.is_inside_visited_link ||
           inside_link_ != EInsideLink::kNotInsideLink);
    if (!checker.Match(context, result)) {
      rejected++;
      continue;
    }
    if (pseudo_style_request_.pseudo_id != kPseudoIdNone &&
        pseudo_style_request_.pseudo_id != result.dynamic_pseudo) {
      rejected++;
      continue;
    }
    if (auto* container_query = rule_data->GetContainerQuery()) {
      result_.SetDependsOnContainerQueries();

      // If we are matching pseudo elements like a ::before rule when computing
      // the styles of the originating element, we don't know whether the
      // container will be the originating element or not. There is not enough
      // information to evaluate the container query for the existence of the
      // pseudo element, so skip the evaluation and have false positives for
      // HasPseudoElementStyles() instead to make sure we create such pseudo
      // elements when they depend on the originating element.
      if (pseudo_style_request_.pseudo_id != kPseudoIdNone ||
          result.dynamic_pseudo == kPseudoIdNone) {
        if (!EvaluateAndAddContainerQueries(*container_query,
                                            style_recalc_context_, result_)) {
          rejected++;
          if (AffectsAnimations(*rule_data))
            result_.SetConditionallyAffectsAnimations();
          continue;
        }
      }
    }

    matched++;
    unsigned layer_order =
        layer_seeker.SeekLayerOrder(rule_data->GetPosition());
    DidMatchRule(rule_data, layer_order, result, match_request);
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

namespace {

base::span<const Attribute> GetAttributes(const Element& element,
                                          bool need_style_synchronized) {
  if (need_style_synchronized) {
    const AttributeCollection collection = element.Attributes();
    return {collection.data(), collection.size()};
  } else {
    const AttributeCollection collection =
        element.AttributesWithoutStyleUpdate();
    return {collection.data(), collection.size()};
  }
}

}  // namespace

DISABLE_CFI_PERF
void ElementRuleCollector::CollectMatchingRules(
    const MatchRequest& match_request) {
  DCHECK(match_request.rule_set);

  SelectorChecker checker(style_.get(), nullptr, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  Element& element = context_.GetElement();
  const AtomicString& pseudo_id = element.ShadowPseudoId();
  if (!pseudo_id.IsEmpty()) {
    DCHECK(element.IsStyledElement());
    CollectMatchingRulesForList(
        match_request.rule_set->UAShadowPseudoElementRules(pseudo_id),
        match_request, checker);
  }

  if (element.IsVTTElement()) {
    CollectMatchingRulesForList(match_request.rule_set->CuePseudoRules(),
                                match_request, checker);
  }
  // Check whether other types of rules are applicable in the current tree
  // scope. Criteria for this:
  // a) the rules are UA rules.
  // b) the rules come from a shadow style sheet in the same tree scope as the
  //    given element.
  // c) is checked in rulesApplicableInCurrentTreeScope.
  if (!matching_ua_rules_ &&
      !RulesApplicableInCurrentTreeScope(&element, match_request.scope))
    return;

  // We need to collect the rules for id, class, tag, and everything else into a
  // buffer and then sort the buffer.
  if (element.HasID()) {
    CollectMatchingRulesForList(
        match_request.rule_set->IdRules(element.IdForStyleResolution()),
        match_request, checker);
  }
  if (element.IsStyledElement() && element.HasClass()) {
    for (wtf_size_t i = 0; i < element.ClassNames().size(); ++i) {
      CollectMatchingRulesForList(
          match_request.rule_set->ClassRules(element.ClassNames()[i]),
          match_request, checker);
    }
  }

  // Collect rules from attribute selector buckets, if we have any.
  if (match_request.rule_set->HasAnyAttrRules()) {
    // HTML documents have case-insensitive attribute matching
    // (so we need to lowercase), non-HTML documents have
    // case-sensitive attribute matching (so we should _not_ lowercase).
    // However, HTML elements already have lowercased their attributes
    // during parsing, so we do not need to do it again.
    const bool lower_attrs_in_default_ns =
        !element.IsHTMLElement() && IsA<HTMLDocument>(element.GetDocument());

    // Due to lazy attributes, this can be a bit tricky. First of all,
    // we need to make sure that if there's a dirty style attribute
    // and there's a ruleset bucket for [style] selectors (which is extremely
    // unusual, but allowed), we check the rules in that bucket.
    // We do this by means of synchronizing the style attribute before
    // iterating, but only if there's actually such a bucket, as it's fairly
    // expensive to do so. (We have a similar issue with SVG attributes,
    // but it is tricky enough to identify if there are any such buckets
    // that we simply always synchronize them if there are any attribute
    // ruleset buckets at all. We can always revisit this if there are any
    // slowdowns from SVG attribute synchronization.)
    //
    // Second, CollectMatchingRulesForList() may call member functions
    // that synchronize the element, adding new attributes to the list
    // while we iterate. These are not relevant for correctness (we
    // would never find any rule buckets matching them anyway),
    // but they may cause reallocation of the vector. For this reason,
    // we cannot use range-based iterators over the attributes here
    // if we don't synchronize before the loop; we need to use
    // simple indexes and then refresh the span after every call.
    bool need_style_synchronized =
        match_request.rule_set->HasBucketForStyleAttribute();
    base::span<const Attribute> attributes =
        GetAttributes(element, need_style_synchronized);

    for (unsigned attr_idx = 0; attr_idx < attributes.size(); ++attr_idx) {
      const AtomicString& attribute_name = attributes[attr_idx].LocalName();
      // NOTE: Attributes in non-default namespaces are case-sensitive.
      // There is a bug where you can set mixed-cased attributes (in non-default
      // namespaces) with setAttributeNS(), but they never match anything.
      // (The relevant code is in AnyAttributeMatches(), in
      // selector_checker.cc.) What we're doing here doesn't influence that bug.
      const AtomicString& lower_name =
          (lower_attrs_in_default_ns &&
           attributes[attr_idx].NamespaceURI() == g_null_atom)
              ? attribute_name.LowerASCII()
              : attribute_name;
      CollectMatchingRulesForList(match_request.rule_set->AttrRules(lower_name),
                                  match_request, checker);

      const AttributeCollection collection = element.AttributesWithoutUpdate();
      attributes = {collection.data(), collection.size()};
    }
  }

  if (element.IsLink()) {
    CollectMatchingRulesForList(match_request.rule_set->LinkPseudoClassRules(),
                                match_request, checker);
  }
  if (inside_link_ != EInsideLink::kNotInsideLink) {
    // Collect rules for visited links regardless of whether they affect
    // rendering to prevent sniffing of visited links via CSS transitions.
    // If the visited or unvisited style changes and an affected property has a
    // transition rule, we create a transition even if it has no visible effect.
    CollectMatchingRulesForList(match_request.rule_set->VisitedDependentRules(),
                                match_request, checker);
  }
  if (SelectorChecker::MatchesFocusPseudoClass(element)) {
    CollectMatchingRulesForList(match_request.rule_set->FocusPseudoClassRules(),
                                match_request, checker);
  }
  if (SelectorChecker::MatchesSelectorFragmentAnchorPseudoClass(element)) {
    CollectMatchingRulesForList(
        match_request.rule_set->SelectorFragmentAnchorRules(), match_request,
        checker);
  }
  if (SelectorChecker::MatchesFocusVisiblePseudoClass(element)) {
    CollectMatchingRulesForList(
        match_request.rule_set->FocusVisiblePseudoClassRules(), match_request,
        checker);
  }
  if (SelectorChecker::MatchesSpatialNavigationInterestPseudoClass(element)) {
    CollectMatchingRulesForList(
        match_request.rule_set->SpatialNavigationInterestPseudoClassRules(),
        match_request, checker);
  }
  AtomicString element_name = matching_ua_rules_
                                  ? element.localName()
                                  : element.LocalNameForSelectorMatching();
  CollectMatchingRulesForList(match_request.rule_set->TagRules(element_name),
                              match_request, checker);
  CollectMatchingRulesForList(match_request.rule_set->UniversalRules(),
                              match_request, checker);
}

void ElementRuleCollector::CollectMatchingShadowHostRules(
    const MatchRequest& match_request) {
  SelectorChecker checker(style_.get(), nullptr, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  CollectMatchingRulesForList(match_request.rule_set->ShadowHostRules(),
                              match_request, checker);
}

void ElementRuleCollector::CollectMatchingSlottedRules(
    const MatchRequest& match_request) {
  SelectorChecker checker(style_.get(), nullptr, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  CollectMatchingRulesForList(
      match_request.rule_set->SlottedPseudoElementRules(), match_request,
      checker);
}

void ElementRuleCollector::CollectMatchingPartPseudoRules(
    const MatchRequest& match_request,
    PartNames& part_names,
    bool for_shadow_pseudo) {
  PartRequest request{part_names, for_shadow_pseudo};
  SelectorChecker checker(style_.get(), &part_names, pseudo_style_request_,
                          mode_, matching_ua_rules_);

  CollectMatchingRulesForList(match_request.rule_set->PartPseudoRules(),
                              match_request, checker, &request);
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

  // For :visited/:link rules, the question of whether or not a selector
  // matches is delayed until cascade-time (see CascadeExpansion), hence such
  // rules may appear to match from ElementRuleCollector's output. This behavior
  // is not correct for Inspector purposes, hence we explicitly filter out
  // rules that don't match the current link state here.
  if (!(rule_data->LinkMatchType() & LinkMatchTypeFromInsideLink(inside_link_)))
    return;

  CSSRule* css_rule = nullptr;
  StyleRule* rule = rule_data->Rule();
  if (parent_style_sheet)
    css_rule = FindStyleRule(parent_style_sheet, rule);
  else
    css_rule = rule->CreateCSSOMWrapper();
  DCHECK(!parent_style_sheet || css_rule);
  EnsureRuleList()->emplace_back(css_rule, rule_data->SelectorIndex());
}

void ElementRuleCollector::SortAndTransferMatchedRules(
    bool is_vtt_embedded_style) {
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
    const MatchedRule& matched_rule = matched_rules_[i];
    const RuleData* rule_data = matched_rule.GetRuleData();
    result_.AddMatchedProperties(
        &rule_data->Rule()->Properties(),
        AddMatchedPropertiesOptions::Builder()
            .SetLinkMatchType(
                AdjustLinkMatchType(inside_link_, rule_data->LinkMatchType()))
            .SetValidPropertyFilter(
                rule_data->GetValidPropertyFilter(matching_ua_rules_))
            .SetLayerOrder(matched_rule.LayerOrder())
            .SetIsInlineStyle(is_vtt_embedded_style)
            .Build());
  }
}

void ElementRuleCollector::DidMatchRule(
    const RuleData* rule_data,
    unsigned layer_order,
    const SelectorChecker::MatchResult& result,
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
    if (!style_ || dynamic_pseudo > kLastTrackedPublicPseudoId)
      return;
    if ((dynamic_pseudo == kPseudoIdBefore ||
         dynamic_pseudo == kPseudoIdAfter) &&
        !rule_data->Rule()->Properties().HasProperty(CSSPropertyID::kContent))
      return;
    if (!rule_data->Rule()->Properties().IsEmpty()) {
      style_->SetHasPseudoElementStyle(dynamic_pseudo);
      if (dynamic_pseudo == kPseudoIdHighlight) {
        DCHECK(result.custom_highlight_name);
        style_->SetHasCustomHighlightName(result.custom_highlight_name);
      }
    }
  } else {
    matched_rules_.push_back(MatchedRule(rule_data, layer_order,
                                         match_request.style_sheet_index,
                                         match_request.style_sheet));
  }
}

static inline bool CompareRules(const MatchedRule& matched_rule1,
                                const MatchedRule& matched_rule2) {
  unsigned layer1 = matched_rule1.LayerOrder();
  unsigned layer2 = matched_rule2.LayerOrder();
  if (layer1 != layer2)
    return layer1 < layer2;

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
