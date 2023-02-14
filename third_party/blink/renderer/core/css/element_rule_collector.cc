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
#include "base/substring_set_matcher/substring_set_matcher.h"
#include "base/trace_event/common/trace_event_common.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_cache_scope.h"
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
#include "third_party/blink/renderer/core/css/selector_checker-inl.h"
#include "third_party/blink/renderer/core/css/selector_statistics.h"
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
  if (inside_link == EInsideLink::kNotInsideLink) {
    return CSSSelector::kMatchLink;
  }
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
    Element* style_container_candidate,
    const ContainerQuery& container_query,
    const StyleRecalcContext& style_recalc_context,
    ContainerSelectorCache& container_selector_cache,
    MatchResult& result) {
  for (const ContainerQuery* current = &container_query; current;
       current = current->Parent()) {
    if (!ContainerQueryEvaluator::EvalAndAdd(
            style_container_candidate, style_recalc_context, *current,
            container_selector_cache, result)) {
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
    if (id == CSSPropertyID::kAll) {
      return true;
    }
    if (id == CSSPropertyID::kVariable) {
      continue;
    }
    if (CSSProperty::Get(id).IsAnimationProperty()) {
      return true;
    }
  }
  return false;
}

// Sequentially scans a sorted list of RuleSet::Interval<T> and seeks
// for the value for a rule (given by its position). Seek() must be called
// with non-decreasing rule positions, so that we only need to go
// through the layer list at most once for all Seek() calls.
template <class T>
class Seeker {
  STACK_ALLOCATED();

 public:
  explicit Seeker(const HeapVector<RuleSet::Interval<T>>& intervals)
      : intervals_(intervals), iter_(intervals_.begin()) {}

  const T* Seek(unsigned rule_position) {
#if DCHECK_IS_ON()
    DCHECK_GE(rule_position, last_rule_position_);
    last_rule_position_ = rule_position;
#endif

    while (iter_ != intervals_.end() &&
           iter_->start_position <= rule_position) {
      ++iter_;
    }
    if (iter_ == intervals_.begin()) {
      return nullptr;
    }
    return std::prev(iter_)->value;
  }

 private:
  const HeapVector<RuleSet::Interval<T>>& intervals_;
  const RuleSet::Interval<T>* iter_;
#if DCHECK_IS_ON()
  unsigned last_rule_position_ = 0;
#endif
};

// A wrapper around Seeker<CascadeLayer> that also translates through the layer
// map.
class CascadeLayerSeeker {
  STACK_ALLOCATED();

 public:
  CascadeLayerSeeker(const ContainerNode* scope,
                     Element* vtt_originating_element,
                     const CSSStyleSheet* style_sheet,
                     const RuleSet* rule_set)
      : seeker_(rule_set->LayerIntervals()),
        layer_map_(FindLayerMap(scope, vtt_originating_element, style_sheet)) {}

  unsigned SeekLayerOrder(unsigned rule_position) {
    if (!layer_map_) {
      return CascadeLayerMap::kImplicitOuterLayerOrder;
    }

    const CascadeLayer* layer = seeker_.Seek(rule_position);
    if (layer == nullptr) {
      return CascadeLayerMap::kImplicitOuterLayerOrder;
    } else {
      return layer_map_->GetLayerOrder(*layer);
    }
  }

 private:
  static const CascadeLayerMap* FindLayerMap(const ContainerNode* scope,
                                             Element* vtt_originating_element,
                                             const CSSStyleSheet* style_sheet) {
    // VTT embedded style is not in any layer.
    if (vtt_originating_element) {
      return nullptr;
    }
    // Assume there are no UA cascade layers, so we only check user layers.
    if (!style_sheet) {
      return nullptr;
    }
    if (scope) {
      DCHECK(scope->ContainingTreeScope().GetScopedStyleResolver());
      return scope->ContainingTreeScope()
          .GetScopedStyleResolver()
          ->GetCascadeLayerMap();
    }
    Document* document = style_sheet->OwnerDocument();
    if (!document) {
      return nullptr;
    }
    return document->GetStyleEngine().GetUserCascadeLayerMap();
  }

  Seeker<CascadeLayer> seeker_;
  const CascadeLayerMap* layer_map_ = nullptr;
};

// The below `rule_map` is designed to aggregate the following values per-rule
// between calls to `DumpAndClearRulesPerfMap`. This is currently done at the
// UpdateStyleAndLayoutTreeForThisDocument level, which yields the statistics
// aggregated across each style recalc pass.
struct CumulativeRulePerfData {
  int match_attempts;
  int fast_reject_count;
  int match_count;
  base::TimeDelta elapsed;
};

using SelectorStatisticsRuleMap =
    HashMap<const RuleData*, CumulativeRulePerfData>;
SelectorStatisticsRuleMap& GetSelectorStatisticsRuleMap() {
  DEFINE_STATIC_LOCAL(SelectorStatisticsRuleMap, rule_map, {});
  return rule_map;
}

void AggregateRulePerfData(
    const HeapVector<RulePerfDataPerRequest>& rules_statistics) {
  SelectorStatisticsRuleMap& map = GetSelectorStatisticsRuleMap();
  for (const auto& rule_stats : rules_statistics) {
    auto it = map.find(rule_stats.rule);
    if (it == map.end()) {
      CumulativeRulePerfData data{
          /*match_attempts*/ 1, (rule_stats.fast_reject) ? 1 : 0,
          (rule_stats.did_match) ? 1 : 0, rule_stats.elapsed};
      map.insert(rule_stats.rule, data);
    } else {
      it->value.elapsed += rule_stats.elapsed;
      it->value.match_attempts++;
      if (rule_stats.fast_reject) {
        it->value.fast_reject_count++;
      }
      if (rule_stats.did_match) {
        it->value.match_count++;
      }
    }
  }
}

// This global caches a pointer to the trace-enabled state for selector
// statistics gathering. This state is global to the process and comes from the
// tracing subsystem. For performance reasons, we only grab the pointer once -
// the value will be updated as tracing is enabled/disabled, which we read by
// dereferencing this global variable. See comment in the definition of
// `TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED` for more details.
static const unsigned char* g_selector_stats_tracing_enabled = nullptr;

}  // namespace

ElementRuleCollector::ElementRuleCollector(
    const ElementResolveContext& context,
    const StyleRecalcContext& style_recalc_context,
    const SelectorFilter& filter,
    MatchResult& result,
    EInsideLink inside_link)
    : context_(context),
      style_recalc_context_(style_recalc_context),
      selector_filter_(filter),
      mode_(SelectorChecker::kResolvingStyle),
      can_use_fast_reject_(
          selector_filter_.ParentStackIsConsistent(context.ParentNode())),
      same_origin_only_(false),
      matching_ua_rules_(false),
      inside_link_(inside_link),
      result_(result) {
  if (!g_selector_stats_tracing_enabled) {
    g_selector_stats_tracing_enabled =
        TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
            TRACE_DISABLED_BY_DEFAULT("blink.debug"));
  }
}

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
  if (!style_rule_list_) {
    style_rule_list_ = MakeGarbageCollected<StyleRuleList>();
  }
  return style_rule_list_;
}

inline RuleIndexList* ElementRuleCollector::EnsureRuleList() {
  if (!css_rule_list_) {
    css_rule_list_ = MakeGarbageCollected<RuleIndexList>();
  }
  return css_rule_list_.Get();
}

void ElementRuleCollector::AddElementStyleProperties(
    const CSSPropertyValueSet* property_set,
    bool is_cacheable,
    bool is_inline_style) {
  if (!property_set) {
    return;
  }
  auto link_match_type = static_cast<unsigned>(CSSSelector::kMatchAll);
  result_.AddMatchedProperties(
      property_set,
      AddMatchedPropertiesOptions::Builder()
          .SetLinkMatchType(AdjustLinkMatchType(inside_link_, link_match_type))
          .SetIsInlineStyle(is_inline_style)
          .Build());
  if (!is_cacheable) {
    result_.SetIsCacheable(false);
  }
}

static bool RulesApplicableInCurrentTreeScope(
    const Element* element,
    const ContainerNode* scoping_node) {
  // Check if the rules come from a shadow style sheet in the same tree scope.
  return !scoping_node ||
         element->ContainingTreeScope() == scoping_node->ContainingTreeScope();
}

bool SlowMatchWithNoResultFlags(
    const SelectorChecker& checker,
    SelectorChecker::SelectorCheckingContext& context,
    const CSSSelector& selector,
    const RuleData& rule_data,
    unsigned expected_proximity = std::numeric_limits<unsigned>::max()) {
  SelectorChecker::MatchResult result;
  context.selector = &selector;
  context.is_inside_visited_link =
      rule_data.LinkMatchType() == CSSSelector::kMatchVisited;
  bool match = checker.Match(context, result);
  DCHECK_EQ(0, result.flags);
  DCHECK_EQ(kPseudoIdNone, result.dynamic_pseudo);
  if (match) {
    DCHECK_EQ(expected_proximity, result.proximity);
  }
  return match;
}

template <bool perf_trace_enabled>
void ElementRuleCollector::CollectMatchingRulesForListInternal(
    base::span<const RuleData> rules,
    const MatchRequest& match_request,
    const RuleSet* rule_set,
    const CSSStyleSheet* style_sheet,
    int style_sheet_index,
    const SelectorChecker& checker,
    PartRequest* part_request) {
  // This StyleScopeFrame is effectively ignored if the StyleRecalcContext
  // provides StyleScopeFrame already (see call to GetParentFrameOrThis below).
  // This happens e.g. when we need to collect matching rules for inspector
  // purposes.
  StyleScopeFrame style_scope_frame(context_.GetElement(),
                                    style_recalc_context_.style_scope_frame);

  SelectorChecker::SelectorCheckingContext context(&context_.GetElement());
  context.scope = match_request.Scope();
  context.pseudo_id = pseudo_style_request_.pseudo_id;
  context.pseudo_argument = &pseudo_style_request_.pseudo_argument;
  context.vtt_originating_element = match_request.VTTOriginatingElement();
  context.style_scope_frame =
      &style_scope_frame.GetParentFrameOrThis(context_.GetElement());
  context.is_initial = !style_recalc_context_.is_ensuring_style &&
                       !style_recalc_context_.old_style;

  CascadeLayerSeeker layer_seeker(
      context.scope, context.vtt_originating_element, style_sheet, rule_set);
  Seeker<ContainerQuery> container_query_seeker(
      rule_set->ContainerQueryIntervals());
  Seeker<StyleScope> scope_seeker(rule_set->ScopeIntervals());

  unsigned fast_rejected = 0;
  unsigned matched = 0;
  SelectorStatisticsCollector selector_statistics_collector;
  if (perf_trace_enabled) {
    selector_statistics_collector.ReserveCapacity(
        static_cast<wtf_size_t>(rules.size()));
  }

  const bool case_sensitive_tag_matching =
      context.element->IsHTMLElement() ||
      !IsA<HTMLDocument>(context.element->GetDocument());

  for (const RuleData& rule_data : rules) {
    if (perf_trace_enabled) {
      selector_statistics_collector.EndCollectionForCurrentRule();
      selector_statistics_collector.BeginCollectionForRule(&rule_data);
    }

    if (can_use_fast_reject_ &&
        selector_filter_.FastRejectSelector<RuleData::kMaximumIdentifierCount>(
            rule_data.DescendantSelectorIdentifierHashes())) {
      fast_rejected++;
      if (perf_trace_enabled) {
        selector_statistics_collector.SetWasFastRejected();
      }
      continue;
    }

    // Don't return cross-origin rules if we did not explicitly ask for them
    // through SetSameOriginOnly.
    if (same_origin_only_ && !rule_data.HasDocumentSecurityOrigin()) {
      continue;
    }

    const auto& selector = rule_data.Selector();
    if (UNLIKELY(part_request && part_request->for_shadow_pseudo)) {
      if (!selector.IsAllowedAfterPart()) {
        DCHECK_EQ(selector.GetPseudoType(), CSSSelector::kPseudoPart);
        continue;
      }
      DCHECK_EQ(selector.Relation(), CSSSelector::kUAShadow);
    }

    SelectorChecker::MatchResult result;
    context.style_scope = scope_seeker.Seek(rule_data.GetPosition());
    if (context.vtt_originating_element == nullptr &&
        rule_data.IsEntirelyCoveredByBucketing()) {
      // Just by seeing this rule, we know that its selector
      // matched, and that we don't get any flags or a match
      // against a pseudo-element. So we can skip the entire test.
      if (pseudo_style_request_.pseudo_id != kPseudoIdNone) {
        continue;
      }
      if (context.style_scope != nullptr &&
          RuntimeEnabledFeatures::CSSScopeEnabled() &&
          !checker.CheckInStyleScope(context, result)) {
        DCHECK(
            !SlowMatchWithNoResultFlags(checker, context, selector, rule_data));
        continue;
      }
      DCHECK(SlowMatchWithNoResultFlags(checker, context, selector, rule_data,
                                        result.proximity));
    } else if (case_sensitive_tag_matching && rule_data.SelectorIsEasy()) {
      if (pseudo_style_request_.pseudo_id != kPseudoIdNone) {
        continue;
      }
      bool easy_match = EasySelectorChecker::Match(&selector, context.element);

      if (context.style_scope != nullptr &&
          RuntimeEnabledFeatures::CSSScopeEnabled() &&
          !checker.CheckInStyleScope(context, result)) {
        easy_match = false;
      }
      DCHECK_EQ(easy_match,
                SlowMatchWithNoResultFlags(checker, context, selector,
                                           rule_data, result.proximity))
          << "Mismatch for selector " << selector.SelectorText()
          << " on element " << context.element;
      if (!easy_match) {
        continue;
      }
    } else {
      context.selector = &selector;
      context.is_inside_visited_link =
          rule_data.LinkMatchType() == CSSSelector::kMatchVisited;
      DCHECK(!context.is_inside_visited_link ||
             inside_link_ != EInsideLink::kNotInsideLink);
      bool match = checker.Match(context, result);
      result_.AddFlags(result.flags);
      if (!match) {
        continue;
      }
      if (pseudo_style_request_.pseudo_id != kPseudoIdNone &&
          pseudo_style_request_.pseudo_id != result.dynamic_pseudo) {
        continue;
      }
    }
    const ContainerQuery* container_query =
        container_query_seeker.Seek(rule_data.GetPosition());
    if (container_query) {
      if (container_query->Selector().SelectsSizeContainers()) {
        result_.SetDependsOnSizeContainerQueries();
      }
      if (container_query->Selector().SelectsStyleContainers()) {
        result_.SetDependsOnStyleContainerQueries();
      }

      // If we are matching pseudo elements like a ::before rule when computing
      // the styles of the originating element, we don't know whether the
      // container will be the originating element or not. There is not enough
      // information to evaluate the container query for the existence of the
      // pseudo element, so skip the evaluation and have false positives for
      // HasPseudoElementStyles() instead to make sure we create such pseudo
      // elements when they depend on the originating element.
      if (pseudo_style_request_.pseudo_id != kPseudoIdNone ||
          result.dynamic_pseudo == kPseudoIdNone) {
        Element* style_container_candidate =
            pseudo_style_request_.pseudo_id == kPseudoIdNone
                ? context_.GetElement().ParentOrShadowHostElement()
                : &context_.GetElement();
        if (!EvaluateAndAddContainerQueries(
                style_container_candidate, *container_query,
                style_recalc_context_, container_selector_cache_, result_)) {
          if (AffectsAnimations(rule_data)) {
            result_.SetConditionallyAffectsAnimations();
          }
          continue;
        }
      }
    }

    matched++;
    if (perf_trace_enabled) {
      selector_statistics_collector.SetDidMatch();
    }
    unsigned layer_order = layer_seeker.SeekLayerOrder(rule_data.GetPosition());
    DidMatchRule(&rule_data, layer_order, container_query, result.proximity,
                 result, style_sheet, style_sheet_index);
  }

  if (perf_trace_enabled) {
    selector_statistics_collector.EndCollectionForCurrentRule();
    AggregateRulePerfData(selector_statistics_collector.PerRuleStatistics());
  }

  StyleEngine& style_engine =
      context_.GetElement().GetDocument().GetStyleEngine();
  if (!style_engine.Stats()) {
    return;
  }

  size_t rejected = rules.size() - fast_rejected - matched;
  INCREMENT_STYLE_STATS_COUNTER(style_engine, rules_rejected, rejected);
  INCREMENT_STYLE_STATS_COUNTER(style_engine, rules_fast_rejected,
                                fast_rejected);
  INCREMENT_STYLE_STATS_COUNTER(style_engine, rules_matched, matched);
}

void ElementRuleCollector::CollectMatchingRulesForList(
    base::span<const RuleData> rules,
    const MatchRequest& match_request,
    const RuleSet* rule_set,
    const CSSStyleSheet* style_sheet,
    int style_sheet_index,
    const SelectorChecker& checker,
    PartRequest* part_request) {
  // This is a very common case for many style sheets, and by putting it here
  // instead of inside CollectMatchingRulesForListInternal(), we're usually
  // inlined into the caller (which saves on stack setup and call overhead
  // in that common case).
  if (rules.empty()) {
    return;
  }

  // To reduce branching overhead for the common case, we use a template
  // parameter to eliminate branching in CollectMatchingRulesForListInternal
  // when tracing is not enabled.
  if (!*g_selector_stats_tracing_enabled) {
    CollectMatchingRulesForListInternal<false>(rules, match_request, rule_set,
                                               style_sheet, style_sheet_index,
                                               checker, part_request);
  } else {
    CollectMatchingRulesForListInternal<true>(rules, match_request, rule_set,
                                              style_sheet, style_sheet_index,
                                              checker, part_request);
  }
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
  DCHECK(!match_request.IsEmpty());

  SelectorChecker checker(nullptr, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  Element& element = context_.GetElement();
  const AtomicString& pseudo_id = element.ShadowPseudoId();
  if (!pseudo_id.empty()) {
    DCHECK(element.IsStyledElement());
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(
          bundle.rule_set->UAShadowPseudoElementRules(pseudo_id), match_request,
          bundle.rule_set, bundle.style_sheet, bundle.style_sheet_index,
          checker);
    }
  }

  if (element.IsVTTElement()) {
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(
          bundle.rule_set->CuePseudoRules(), match_request, bundle.rule_set,
          bundle.style_sheet, bundle.style_sheet_index, checker);
    }
  }
  // Check whether other types of rules are applicable in the current tree
  // scope. Criteria for this:
  // a) the rules are UA rules.
  // b) the rules come from a shadow style sheet in the same tree scope as the
  //    given element.
  // c) is checked in rulesApplicableInCurrentTreeScope.
  if (!matching_ua_rules_ &&
      !RulesApplicableInCurrentTreeScope(&element, match_request.Scope())) {
    return;
  }

  // We need to collect the rules for id, class, tag, and everything else into a
  // buffer and then sort the buffer.
  if (element.HasID()) {
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(
          bundle.rule_set->IdRules(element.IdForStyleResolution()),
          match_request, bundle.rule_set, bundle.style_sheet,
          bundle.style_sheet_index, checker);
    }
  }
  if (element.IsStyledElement() && element.HasClass()) {
    for (wtf_size_t i = 0; i < element.ClassNames().size(); ++i) {
      for (const auto bundle : match_request.AllRuleSets()) {
        CollectMatchingRulesForList(
            bundle.rule_set->ClassRules(element.ClassNames()[i]), match_request,
            bundle.rule_set, bundle.style_sheet, bundle.style_sheet_index,
            checker);
      }
    }
  }

  // Collect rules from attribute selector buckets, if we have any.
  bool has_any_attr_rules = false;
  bool need_style_synchronized = false;
  for (const auto bundle : match_request.AllRuleSets()) {
    if (bundle.rule_set->HasAnyAttrRules()) {
      has_any_attr_rules = true;
      if (bundle.rule_set->HasBucketForStyleAttribute()) {
        need_style_synchronized = true;
      }
    }
  }
  if (has_any_attr_rules) {
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
    base::span<const Attribute> attributes =
        GetAttributes(element, need_style_synchronized);

    for (unsigned attr_idx = 0; attr_idx < attributes.size(); ++attr_idx) {
      const AtomicString& attribute_name = attributes[attr_idx].LocalName();
      // NOTE: Attributes in non-default namespaces are case-sensitive.
      // There is a bug where you can set mixed-cased attributes (in
      // non-default namespaces) with setAttributeNS(), but they never match
      // anything. (The relevant code is in AnyAttributeMatches(), in
      // selector_checker.cc.) What we're doing here doesn't influence that
      // bug.
      const AtomicString& lower_name =
          (lower_attrs_in_default_ns &&
           attributes[attr_idx].NamespaceURI() == g_null_atom)
              ? attribute_name.LowerASCII()
              : attribute_name;
      for (const auto bundle : match_request.AllRuleSets()) {
        if (bundle.rule_set->HasAnyAttrRules()) {
          base::span<const RuleData> list =
              bundle.rule_set->AttrRules(lower_name);
          if (!list.empty() &&
              !bundle.rule_set->CanIgnoreEntireList(
                  list, lower_name, attributes[attr_idx].Value())) {
            CollectMatchingRulesForList(bundle.rule_set->AttrRules(lower_name),
                                        match_request, bundle.rule_set,
                                        bundle.style_sheet,
                                        bundle.style_sheet_index, checker);
          }
        }
      }

      const AttributeCollection collection = element.AttributesWithoutUpdate();
      attributes = {collection.data(), collection.size()};
    }
  }

  if (element.IsLink()) {
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(bundle.rule_set->LinkPseudoClassRules(),
                                  match_request, bundle.rule_set,
                                  bundle.style_sheet, bundle.style_sheet_index,
                                  checker);
    }
  }
  if (inside_link_ != EInsideLink::kNotInsideLink) {
    // Collect rules for visited links regardless of whether they affect
    // rendering to prevent sniffing of visited links via CSS transitions.
    // If the visited or unvisited style changes and an affected property has
    // a transition rule, we create a transition even if it has no visible
    // effect.
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(bundle.rule_set->VisitedDependentRules(),
                                  match_request, bundle.rule_set,
                                  bundle.style_sheet, bundle.style_sheet_index,
                                  checker);
    }
  }
  if (SelectorChecker::MatchesFocusPseudoClass(element)) {
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(bundle.rule_set->FocusPseudoClassRules(),
                                  match_request, bundle.rule_set,
                                  bundle.style_sheet, bundle.style_sheet_index,
                                  checker);
    }
  }
  if (SelectorChecker::MatchesSelectorFragmentAnchorPseudoClass(element)) {
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(
          bundle.rule_set->SelectorFragmentAnchorRules(), match_request,
          bundle.rule_set, bundle.style_sheet, bundle.style_sheet_index,
          checker);
    }
  }
  if (SelectorChecker::MatchesFocusVisiblePseudoClass(element)) {
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(
          bundle.rule_set->FocusVisiblePseudoClassRules(), match_request,
          bundle.rule_set, bundle.style_sheet, bundle.style_sheet_index,
          checker);
    }
  }
  if (SelectorChecker::MatchesSpatialNavigationInterestPseudoClass(element)) {
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(
          bundle.rule_set->SpatialNavigationInterestPseudoClassRules(),
          match_request, bundle.rule_set, bundle.style_sheet,
          bundle.style_sheet_index, checker);
    }
  }
  if (element.GetDocument().documentElement() == element) {
    for (const auto bundle : match_request.AllRuleSets()) {
      CollectMatchingRulesForList(
          bundle.rule_set->RootElementRules(), match_request, bundle.rule_set,
          bundle.style_sheet, bundle.style_sheet_index, checker);
    }
  }
  AtomicString element_name = matching_ua_rules_
                                  ? element.localName()
                                  : element.LocalNameForSelectorMatching();
  for (const auto bundle : match_request.AllRuleSets()) {
    CollectMatchingRulesForList(
        bundle.rule_set->TagRules(element_name), match_request, bundle.rule_set,
        bundle.style_sheet, bundle.style_sheet_index, checker);
  }
  for (const auto bundle : match_request.AllRuleSets()) {
    CollectMatchingRulesForList(
        bundle.rule_set->UniversalRules(), match_request, bundle.rule_set,
        bundle.style_sheet, bundle.style_sheet_index, checker);
  }
}

void ElementRuleCollector::CollectMatchingShadowHostRules(
    const MatchRequest& match_request) {
  SelectorChecker checker(nullptr, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  for (const auto bundle : match_request.AllRuleSets()) {
    CollectMatchingRulesForList(
        bundle.rule_set->ShadowHostRules(), match_request, bundle.rule_set,
        bundle.style_sheet, bundle.style_sheet_index, checker);
  }
}

void ElementRuleCollector::CollectMatchingSlottedRules(
    const MatchRequest& match_request) {
  SelectorChecker checker(nullptr, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  for (const auto bundle : match_request.AllRuleSets()) {
    CollectMatchingRulesForList(
        bundle.rule_set->SlottedPseudoElementRules(), match_request,
        bundle.rule_set, bundle.style_sheet, bundle.style_sheet_index, checker);
  }
}

void ElementRuleCollector::CollectMatchingPartPseudoRules(
    const MatchRequest& match_request,
    PartNames& part_names,
    bool for_shadow_pseudo) {
  PartRequest request{part_names, for_shadow_pseudo};
  SelectorChecker checker(&part_names, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  for (const auto bundle : match_request.AllRuleSets()) {
    CollectMatchingRulesForList(
        bundle.rule_set->PartPseudoRules(), match_request, bundle.rule_set,
        bundle.style_sheet, bundle.style_sheet_index, checker, &request);
  }
}

template <class CSSRuleCollection>
CSSRule* ElementRuleCollector::FindStyleRule(CSSRuleCollection* css_rules,
                                             StyleRule* style_rule) {
  if (!css_rules) {
    return nullptr;
  }

  for (unsigned i = 0; i < css_rules->length(); ++i) {
    CSSRule* css_rule = css_rules->item(i);
    if (auto* css_style_rule = DynamicTo<CSSStyleRule>(css_rule)) {
      if (css_style_rule->GetStyleRule() == style_rule) {
        return css_rule;
      }
      if (CSSRule* result =
              FindStyleRule(css_style_rule->cssRules(), style_rule);
          result) {
        return result;
      }
    } else if (auto* css_import_rule = DynamicTo<CSSImportRule>(css_rule)) {
      if (CSSRule* result =
              FindStyleRule(css_import_rule->styleSheet(), style_rule);
          result) {
        return result;
      }
    } else if (CSSRule* result =
                   FindStyleRule(css_rule->cssRules(), style_rule);
               result) {
      return result;
    }
  }
  return nullptr;
}

void ElementRuleCollector::AppendCSSOMWrapperForRule(
    CSSStyleSheet* parent_style_sheet,
    const RuleData* rule_data,
    wtf_size_t position) {
  // |parentStyleSheet| is 0 if and only if the |rule| is coming from User
  // Agent. In this case, it is safe to create CSSOM wrappers without
  // parentStyleSheets as they will be used only by inspector which will not try
  // to edit them.

  // For :visited/:link rules, the question of whether or not a selector
  // matches is delayed until cascade-time (see CascadeExpansion), hence such
  // rules may appear to match from ElementRuleCollector's output. This behavior
  // is not correct for Inspector purposes, hence we explicitly filter out
  // rules that don't match the current link state here.
  if (!(rule_data->LinkMatchType() &
        LinkMatchTypeFromInsideLink(inside_link_))) {
    return;
  }

  CSSRule* css_rule = nullptr;
  StyleRule* rule = rule_data->Rule();
  if (parent_style_sheet) {
    css_rule = FindStyleRule(parent_style_sheet, rule);
  } else {
    css_rule = rule->CreateCSSOMWrapper(position);
  }
  DCHECK(!parent_style_sheet || css_rule);
  EnsureRuleList()->emplace_back(css_rule, rule_data->SelectorIndex());
}

void ElementRuleCollector::SortAndTransferMatchedRules(
    bool is_vtt_embedded_style) {
  if (matched_rules_.empty()) {
    return;
  }

  SortMatchedRules();

  if (mode_ == SelectorChecker::kCollectingStyleRules) {
    for (unsigned i = 0; i < matched_rules_.size(); ++i) {
      EnsureStyleRuleList()->push_back(matched_rules_[i].GetRuleData()->Rule());
    }
    return;
  }

  if (mode_ == SelectorChecker::kCollectingCSSRules) {
    for (unsigned i = 0; i < matched_rules_.size(); ++i) {
      AppendCSSOMWrapperForRule(
          const_cast<CSSStyleSheet*>(matched_rules_[i].ParentStyleSheet()),
          matched_rules_[i].GetRuleData(), i);
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
    const ContainerQuery* container_query,
    unsigned proximity,
    const SelectorChecker::MatchResult& result,
    const CSSStyleSheet* style_sheet,
    int style_sheet_index) {
  PseudoId dynamic_pseudo = result.dynamic_pseudo;
  // If we're matching normal rules, set a pseudo bit if we really just matched
  // a pseudo-element.
  if (dynamic_pseudo != kPseudoIdNone &&
      pseudo_style_request_.pseudo_id == kPseudoIdNone) {
    if (mode_ == SelectorChecker::kCollectingCSSRules ||
        mode_ == SelectorChecker::kCollectingStyleRules) {
      return;
    }
    if (dynamic_pseudo > kLastTrackedPublicPseudoId) {
      return;
    }
    if ((dynamic_pseudo == kPseudoIdBefore ||
         dynamic_pseudo == kPseudoIdAfter) &&
        !rule_data->Rule()->Properties().HasProperty(CSSPropertyID::kContent)) {
      return;
    }
    if (rule_data->Rule()->Properties().IsEmpty()) {
      return;
    }

    result_.SetHasPseudoElementStyle(dynamic_pseudo);

    if (IsHighlightPseudoElement(dynamic_pseudo)) {
      // Determine whether the selector definitely matches the highlight pseudo
      // of all elements, without any namespace limits or other conditions.
      bool universal = false;
      const CSSSelector& selector = rule_data->Selector();
      if (CSSSelector::GetPseudoId(selector.GetPseudoType()) ==
          dynamic_pseudo) {
        // When there is no default @namespace, *::selection and *|*::selection
        // are stored without the star, so we are universal if there’s nothing
        // before (e.g. x::selection) and nothing after (e.g. y ::selection).
        universal = selector.IsLastInTagHistory();
      } else if (const CSSSelector* next = selector.TagHistory()) {
        // When there is a default @namespace, ::selection and *::selection (not
        // universal) are stored as g_null_atom|*::selection, |*::selection (not
        // universal) is stored as g_empty_atom|*::selection, and *|*::selection
        // (the only universal form) is stored as g_star_atom|*::selection.
        universal =
            next->IsLastInTagHistory() &&
            CSSSelector::GetPseudoId(next->GetPseudoType()) == dynamic_pseudo &&
            selector.Match() == CSSSelector::kTag &&
            selector.TagQName().LocalName().IsNull() &&
            selector.TagQName().Prefix() == g_star_atom;
      }

      if (!universal) {
        result_.SetHasNonUniversalHighlightPseudoStyles();
      }

      if (!matching_ua_rules_) {
        result_.SetHasNonUaHighlightPseudoStyles();
      }

      if (dynamic_pseudo == kPseudoIdHighlight) {
        DCHECK(result.custom_highlight_name);
        result_.AddCustomHighlightName(
            AtomicString(result.custom_highlight_name));
      }
    } else if (dynamic_pseudo == kPseudoIdFirstLine && container_query) {
      result_.SetFirstLineDependsOnSizeContainerQueries();
    }
  } else {
    matched_rules_.push_back(MatchedRule(rule_data, layer_order, proximity,
                                         style_sheet_index, style_sheet));
  }
}

void ElementRuleCollector::DumpAndClearRulesPerfMap() {
  TRACE_EVENT1(
      TRACE_DISABLED_BY_DEFAULT("blink.debug"), "SelectorStats",
      "selector_stats", [&](perfetto::TracedValue context) {
        perfetto::TracedDictionary dict = std::move(context).WriteDictionary();
        {
          perfetto::TracedArray array = dict.AddArray("selector_timings");
          for (auto& it : GetSelectorStatisticsRuleMap()) {
            perfetto::TracedValue item = array.AppendItem();
            perfetto::TracedDictionary item_dict =
                std::move(item).WriteDictionary();
            const CSSSelector& selector = it.key->Selector();
            item_dict.Add("selector", selector.SelectorText());
            item_dict.Add("elapsed (us)", it.value.elapsed);
            item_dict.Add("match_attempts", it.value.match_attempts);
            item_dict.Add("fast_reject_count", it.value.fast_reject_count);
            item_dict.Add("match_count", it.value.match_count);
          }
        }
      });
  GetSelectorStatisticsRuleMap().clear();
}

inline bool ElementRuleCollector::CompareRules(
    const MatchedRule& matched_rule1,
    const MatchedRule& matched_rule2) {
  unsigned layer1 = matched_rule1.LayerOrder();
  unsigned layer2 = matched_rule2.LayerOrder();
  if (layer1 != layer2) {
    return layer1 < layer2;
  }

  unsigned specificity1 = matched_rule1.Specificity();
  unsigned specificity2 = matched_rule2.Specificity();
  if (specificity1 != specificity2) {
    return specificity1 < specificity2;
  }

  unsigned proximity1 = matched_rule1.Proximity();
  unsigned proximity2 = matched_rule2.Proximity();
  if (proximity1 != proximity2) {
    return proximity1 > proximity2;
  }

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
