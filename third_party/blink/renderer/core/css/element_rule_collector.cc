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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/element_rule_collector.h"

#include <utility>

#include "base/containers/span.h"
#include "base/substring_set_matcher/substring_set_matcher.h"
#include "base/trace_event/common/trace_event_common.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-shared.h"
#include "third_party/blink/renderer/core/css/cascade_layer_map.h"
#include "third_party/blink/renderer/core/css/check_pseudo_has_cache_scope.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/css_import_rule.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_media_rule.h"
#include "third_party/blink/renderer/core/css/css_nested_declarations_rule.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/css_rule_list.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_style_rule.h"
#include "third_party/blink/renderer/core/css/css_style_sheet.h"
#include "third_party/blink/renderer/core/css/css_supports_rule.h"
#include "third_party/blink/renderer/core/css/resolver/element_resolve_context.h"
#include "third_party/blink/renderer/core/css/resolver/scoped_style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_stats.h"
#include "third_party/blink/renderer/core/css/resolver/style_rule_usage_tracker.h"
#include "third_party/blink/renderer/core/css/seeker.h"
#include "third_party/blink/renderer/core/css/selector_checker-inl.h"
#include "third_party/blink/renderer/core/css/selector_statistics.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_rule_nested_declarations.h"
#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/page/scrolling/fragment_anchor.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace {

struct CumulativeRulePerfKey {
  String selector;
  String style_sheet_id;
  CumulativeRulePerfKey(const String& selector, const String& style_sheet_id)
      : selector(selector), style_sheet_id(style_sheet_id) {}
};

struct ContextWithStyleScopeFrame {
  STACK_ALLOCATED();

 public:
  ContextWithStyleScopeFrame(Element* element,
                             const MatchRequest& match_request,
                             StyleRequest* pseudo_style_request,
                             StyleScopeFrame* parent_frame)
      : style_scope_frame(*element, parent_frame), context(element) {
    context.style_scope_frame =
        &style_scope_frame.GetParentFrameOrThis(*element);
    context.scope = match_request.Scope();
    context.pseudo_id = pseudo_style_request->pseudo_id;
    context.pseudo_argument = &pseudo_style_request->pseudo_argument;
    context.vtt_originating_element = match_request.VTTOriginatingElement();
    switch (pseudo_style_request->search_text_request) {
      case StyleRequest::kNone:
        DCHECK_NE(context.pseudo_id, kPseudoIdSearchText);
        break;
      case StyleRequest::kCurrent:
        context.search_text_request_is_current = true;
        break;
      case StyleRequest::kNotCurrent:
        context.search_text_request_is_current = false;
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  // This StyleScopeFrame is effectively ignored if the StyleRecalcContext
  // provides StyleScopeFrame already (see call to GetParentFrameOrThis above).
  // This happens e.g. when we need to collect matching rules for inspector
  // purposes.
  StyleScopeFrame style_scope_frame;
  SelectorChecker::SelectorCheckingContext context;
};

}  // namespace
}  // namespace blink

namespace WTF {
template <>
struct HashTraits<blink::CumulativeRulePerfKey>
    : TwoFieldsHashTraits<blink::CumulativeRulePerfKey,
                          &blink::CumulativeRulePerfKey::selector,
                          &blink::CumulativeRulePerfKey::style_sheet_id> {};
}  // namespace WTF

namespace blink {

template <class CSSRuleCollection>
static CSSRule* FindStyleRule(CSSRuleCollection* css_rules,
                              const StyleRule* style_rule);

namespace {

const CSSStyleSheet* FindStyleSheet(const TreeScope* tree_scope_containing_rule,
                                    const StyleEngine& style_engine,
                                    const StyleRule* rule) {
  if (tree_scope_containing_rule) {
    for (const auto& [sheet, rule_set] :
         tree_scope_containing_rule->GetScopedStyleResolver()
             ->GetActiveStyleSheets()) {
      if (FindStyleRule(sheet.Get(), rule) != nullptr) {
        return sheet.Get();
      }
    }
  }
  for (const auto& [sheet, rule_set] : style_engine.ActiveUserStyleSheets()) {
    if (FindStyleRule(sheet.Get(), rule) != nullptr) {
      return sheet.Get();
    }
  }

  return nullptr;  // Not found (e.g., the rule is from an UA style sheet).
}

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

// A wrapper around Seeker<CascadeLayer> that also translates through the layer
// map.
class CascadeLayerSeeker {
  STACK_ALLOCATED();

 public:
  CascadeLayerSeeker(const ContainerNode* scope,
                     Element* vtt_originating_element,
                     bool matching_ua_rules,
                     bool matching_rules_from_no_style_sheet,
                     const Document* document,
                     const RuleSet* rule_set)
      : seeker_(rule_set->LayerIntervals()),
        layer_map_(FindLayerMap(scope,
                                vtt_originating_element,
                                matching_ua_rules,
                                matching_rules_from_no_style_sheet,
                                document)) {}

  uint16_t SeekLayerOrder(unsigned rule_position) {
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
  static const CascadeLayerMap* FindLayerMap(
      const ContainerNode* scope,
      Element* vtt_originating_element,
      bool matching_ua_rules,
      bool matching_rules_from_no_style_sheet,
      const Document* document) {
    // VTT embedded style is not in any layer.
    if (vtt_originating_element) {
      return nullptr;
    }
    // Assume there are no UA cascade layers, so we only check user layers.
    if (matching_ua_rules || matching_rules_from_no_style_sheet) {
      return nullptr;
    }
    if (scope) {
      DCHECK(scope->IsInTreeScope());
      DCHECK(scope->GetTreeScope().GetScopedStyleResolver());
      return scope->GetTreeScope()
          .GetScopedStyleResolver()
          ->GetCascadeLayerMap();
    }
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
    HashMap<CumulativeRulePerfKey, CumulativeRulePerfData>;
SelectorStatisticsRuleMap& GetSelectorStatisticsRuleMap() {
  DEFINE_STATIC_LOCAL(SelectorStatisticsRuleMap, rule_map, {});
  return rule_map;
}

void AggregateRulePerfData(
    const TreeScope* tree_scope_containing_rule,
    const StyleEngine& style_engine,
    const Vector<RulePerfDataPerRequest>& rules_statistics) {
  SelectorStatisticsRuleMap& map = GetSelectorStatisticsRuleMap();
  for (const auto& rule_stats : rules_statistics) {
    const RuleData* rule = rule_stats.rule;
    const CSSStyleSheet* style_sheet =
        FindStyleSheet(tree_scope_containing_rule, style_engine, rule->Rule());
    CumulativeRulePerfKey key{
        rule->Selector().SelectorText(),
        IdentifiersFactory::IdForCSSStyleSheet(style_sheet)};
    auto it = map.find(key);
    if (it == map.end()) {
      CumulativeRulePerfData data{
          /*match_attempts*/ 1, (rule_stats.fast_reject) ? 1 : 0,
          (rule_stats.did_match) ? 1 : 0, rule_stats.elapsed};
      map.insert(key, data);
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
          selector_filter_.ParentStackIsConsistent(context.ParentElement())),
      matching_ua_rules_(false),
      suppress_visited_(false),
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
  return style_rule_list_.Get();
}

inline RuleIndexList* ElementRuleCollector::EnsureRuleList() {
  if (!css_rule_list_) {
    css_rule_list_ = MakeGarbageCollected<RuleIndexList>();
  }
  return css_rule_list_.Get();
}

void ElementRuleCollector::AddElementStyleProperties(
    const CSSPropertyValueSet* property_set,
    CascadeOrigin origin,
    bool is_cacheable,
    bool is_inline_style) {
  if (!property_set) {
    return;
  }
  auto link_match_type = static_cast<unsigned>(CSSSelector::kMatchAll);
  result_.AddMatchedProperties(
      property_set, {.link_match_type = static_cast<uint8_t>(
                         AdjustLinkMatchType(inside_link_, link_match_type)),
                     .is_inline_style = is_inline_style,
                     .origin = origin});
  if (!is_cacheable) {
    result_.SetIsCacheable(false);
  }
}

void ElementRuleCollector::AddTryStyleProperties() {
  const CSSPropertyValueSet* property_set = style_recalc_context_.try_set;
  if (!property_set) {
    return;
  }
  auto link_match_type = static_cast<unsigned>(CSSSelector::kMatchAll);
  result_.AddMatchedProperties(
      property_set, {.link_match_type = static_cast<uint8_t>(
                         AdjustLinkMatchType(inside_link_, link_match_type)),
                     .valid_property_filter = static_cast<uint8_t>(
                         ValidPropertyFilter::kPositionTry),
                     .is_try_style = true,
                     .origin = CascadeOrigin::kAuthor});
  result_.SetIsCacheable(false);
}

void ElementRuleCollector::AddTryTacticsStyleProperties() {
  const CSSPropertyValueSet* property_set =
      style_recalc_context_.try_tactics_set;
  if (!property_set) {
    return;
  }
  auto link_match_type = static_cast<unsigned>(CSSSelector::kMatchAll);
  result_.AddMatchedProperties(
      property_set, {.link_match_type = static_cast<uint8_t>(
                         AdjustLinkMatchType(inside_link_, link_match_type)),
                     .origin = CascadeOrigin::kAuthor,
                     .is_try_tactics_style = true});
  result_.SetIsCacheable(false);
}

static bool RulesApplicableInCurrentTreeScope(
    const Element* element,
    const ContainerNode* scoping_node) {
  // Check if the rules come from a shadow style sheet in the same tree scope.
  DCHECK(element->IsInTreeScope());
  return !scoping_node ||
         element->GetTreeScope() == scoping_node->GetTreeScope();
}

bool SlowMatchWithNoResultFlags(
    const SelectorChecker& checker,
    SelectorChecker::SelectorCheckingContext& context,
    const CSSSelector& selector,
    const RuleData& rule_data,
    bool suppress_visited,
    unsigned expected_proximity = std::numeric_limits<unsigned>::max()) {
  SelectorChecker::MatchResult result;
  context.selector = &selector;
  context.match_visited = !suppress_visited && rule_data.LinkMatchType() ==
                                                   CSSSelector::kMatchVisited;
  bool match = checker.Match(context, result);
  DCHECK_EQ(0, result.flags);
  DCHECK_EQ(kPseudoIdNone, result.dynamic_pseudo);
  if (match) {
    DCHECK_EQ(expected_proximity, result.proximity);
  }
  return match;
}

template <bool stop_at_first_match, bool perf_trace_enabled>
bool ElementRuleCollector::CollectMatchingRulesForListInternal(
    base::span<const RuleData> rules,
    const MatchRequest& match_request,
    const RuleSet* rule_set,
    int style_sheet_index,
    const SelectorChecker& checker,
    SelectorChecker::SelectorCheckingContext& context,
    PartRequest* part_request) {
  // If we are _not_ in initial style, or we are just collecting rules,
  // we must skip all rules marked with @starting-style.
  bool reject_starting_styles = style_recalc_context_.is_ensuring_style ||
                                style_recalc_context_.old_style ||
                                mode_ != SelectorChecker::kResolvingStyle;

  CascadeLayerSeeker layer_seeker(stop_at_first_match ? nullptr : context.scope,
                                  context.vtt_originating_element,
                                  matching_ua_rules_,
                                  matching_rules_from_no_style_sheet_,
                                  &context.element->GetDocument(), rule_set);
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

  for (const RuleData& rule_data : rules) {
    if (perf_trace_enabled) {
      selector_statistics_collector.EndCollectionForCurrentRule();
      selector_statistics_collector.BeginCollectionForRule(&rule_data);
    }
    if (can_use_fast_reject_ &&
        selector_filter_.FastRejectSelector(
            rule_data.DescendantSelectorIdentifierHashes(
                rule_set->BloomHashBacking()))) {
      fast_rejected++;
      if (perf_trace_enabled) {
        selector_statistics_collector.SetWasFastRejected();
      }
      continue;
    }

    const auto& selector = rule_data.Selector();
    if (part_request && part_request->for_shadow_pseudo) [[unlikely]] {
      if (!selector.IsAllowedAfterPart()) {
        DCHECK_EQ(selector.GetPseudoType(), CSSSelector::kPseudoPart);
        continue;
      }
      DCHECK_EQ(selector.Relation(), CSSSelector::kUAShadow);
    }

    if (reject_starting_styles && rule_data.IsStartingStyle()) {
      continue;
    }

    // We cannot use easy selector matching for VTT elements.
    // It is also not prepared to deal with the featurelessness
    // of the host (see comment in SelectorChecker::CheckOne()).
    bool can_use_easy_selector_matching =
        context.vtt_originating_element == nullptr &&
        !(context.scope && context.scope->OwnerShadowHost() == context.element);

    SelectorChecker::MatchResult result;
    if (can_use_easy_selector_matching &&
        rule_data.IsEntirelyCoveredByBucketing()) {
      // Just by seeing this rule, we know that its selector
      // matched, and that we don't get any flags or a match
      // against a pseudo-element. So we can skip the entire test.
      if (pseudo_style_request_.pseudo_id != kPseudoIdNone) {
        continue;
      }
#if DCHECK_IS_ON()
      context.style_scope = scope_seeker.Seek(rule_data.GetPosition());
      DCHECK(!context.style_scope);
      DCHECK(SlowMatchWithNoResultFlags(checker, context, selector, rule_data,
                                        suppress_visited_, result.proximity));
#endif
    } else if (can_use_easy_selector_matching && rule_data.SelectorIsEasy()) {
      if (pseudo_style_request_.pseudo_id != kPseudoIdNone) {
        continue;
      }
      bool easy_match = EasySelectorChecker::Match(&selector, context.element);
#if DCHECK_IS_ON()
      context.style_scope = scope_seeker.Seek(rule_data.GetPosition());
      DCHECK(!context.style_scope);
      DCHECK_EQ(easy_match, SlowMatchWithNoResultFlags(
                                checker, context, selector, rule_data,
                                suppress_visited_, result.proximity))
          << "Mismatch for selector " << selector.SelectorText()
          << " on element " << context.element;
#endif
      if (!easy_match) {
        continue;
      }
    } else {
      context.selector = &selector;
      context.match_visited =
          !suppress_visited_ &&
          rule_data.LinkMatchType() == CSSSelector::kMatchVisited;
      context.style_scope = scope_seeker.Seek(rule_data.GetPosition());
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
    if (stop_at_first_match) {
      return true;
    }
    const ContainerQuery* container_query =
        container_query_seeker.Seek(rule_data.GetPosition());
    if (container_query) {
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
            style_recalc_context_.style_container;
        if (!style_container_candidate) {
          if (pseudo_style_request_.pseudo_id == kPseudoIdNone) {
            style_container_candidate =
                ContainerQueryEvaluator::ParentContainerCandidateElement(
                    context_.GetElement());
          } else {
            style_container_candidate = &context_.GetElement();
          }
        }
        if (!EvaluateAndAddContainerQueries(
                style_container_candidate, *container_query,
                style_recalc_context_, container_selector_cache_, result_)) {
          if (AffectsAnimations(rule_data)) {
            result_.SetConditionallyAffectsAnimations();
          }
          continue;
        }
      } else {
        // We are skipping container query matching for pseudo element selectors
        // when not actually matching style for the pseudo element itself. Still
        // we need to keep track of size/style query dependencies since query
        // changes may cause pseudo elements to start being generated.
        bool selects_size = false;
        bool selects_style = false;
        bool selects_sticky = false;
        for (const ContainerQuery* current = container_query; current;
             current = current->Parent()) {
          selects_size |= current->Selector().SelectsSizeContainers();
          selects_style |= current->Selector().SelectsStyleContainers();
          selects_sticky |= current->Selector().SelectsStickyContainers();
        }
        if (selects_size) {
          result_.SetDependsOnSizeContainerQueries();
        }
        if (selects_style) {
          result_.SetDependsOnStyleContainerQueries();
        }
        if (selects_sticky) {
          result_.SetDependsOnStateContainerQueries();
        }
      }
    }

    matched++;
    if (perf_trace_enabled) {
      selector_statistics_collector.SetDidMatch();
    }
    unsigned layer_order = layer_seeker.SeekLayerOrder(rule_data.GetPosition());
    DidMatchRule(&rule_data, layer_order, container_query, result.proximity,
                 result, style_sheet_index);
  }

  if (perf_trace_enabled) {
    DCHECK_EQ(mode_, SelectorChecker::kResolvingStyle);
    selector_statistics_collector.EndCollectionForCurrentRule();
    AggregateRulePerfData(current_matching_tree_scope_,
                          context_.GetElement().GetDocument().GetStyleEngine(),
                          selector_statistics_collector.PerRuleStatistics());
  }

  StyleEngine& style_engine =
      context_.GetElement().GetDocument().GetStyleEngine();
  if (!style_engine.Stats()) {
    return false;
  }

  size_t rejected = rules.size() - fast_rejected - matched;
  INCREMENT_STYLE_STATS_COUNTER(style_engine, rules_rejected, rejected);
  INCREMENT_STYLE_STATS_COUNTER(style_engine, rules_fast_rejected,
                                fast_rejected);
  INCREMENT_STYLE_STATS_COUNTER(style_engine, rules_matched, matched);
  return false;
}

template <bool stop_at_first_match>
bool ElementRuleCollector::CollectMatchingRulesForList(
    base::span<const RuleData> rules,
    const MatchRequest& match_request,
    const RuleSet* rule_set,
    int style_sheet_index,
    const SelectorChecker& checker,
    SelectorChecker::SelectorCheckingContext& context,
    PartRequest* part_request) {
  // This is a very common case for many style sheets, and by putting it here
  // instead of inside CollectMatchingRulesForListInternal(), we're usually
  // inlined into the caller (which saves on stack setup and call overhead
  // in that common case).
  if (rules.empty()) {
    return false;
  }

  // To reduce branching overhead for the common case, we use a template
  // parameter to eliminate branching in CollectMatchingRulesForListInternal
  // when tracing is not enabled.
  if (!*g_selector_stats_tracing_enabled) {
    return CollectMatchingRulesForListInternal<stop_at_first_match, false>(
        rules, match_request, rule_set, style_sheet_index, checker, context,
        part_request);
  } else {
    return CollectMatchingRulesForListInternal<stop_at_first_match, true>(
        rules, match_request, rule_set, style_sheet_index, checker, context,
        part_request);
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

void ElementRuleCollector::CollectMatchingRules(
    const MatchRequest& match_request,
    PartNames* part_names) {
  CollectMatchingRulesInternal</*stop_at_first_match=*/false>(match_request,
                                                              part_names);
}

DISABLE_CFI_PERF
bool ElementRuleCollector::CheckIfAnyRuleMatches(
    const MatchRequest& match_request) {
  return CollectMatchingRulesInternal</*stop_at_first_match=*/true>(
      match_request, /*part_names*/ nullptr);
}

bool ElementRuleCollector::CanRejectScope(const StyleScope& style_scope) {
  if (!style_scope.IsImplicit()) {
    return false;
  }
  StyleScopeFrame* style_scope_frame = style_recalc_context_.style_scope_frame;
  return style_scope_frame &&
         !style_scope_frame->HasSeenImplicitScope(style_scope);
}

template <bool stop_at_first_match>
DISABLE_CFI_PERF bool ElementRuleCollector::CollectMatchingRulesInternal(
    const MatchRequest& match_request,
    PartNames* part_names) {
  DCHECK(!match_request.IsEmpty());

  SelectorChecker checker(part_names, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  ContextWithStyleScopeFrame context(&context_.GetElement(), match_request,
                                     &pseudo_style_request_,
                                     style_recalc_context_.style_scope_frame);

  Element& element = context_.GetElement();
  const AtomicString& pseudo_id = element.ShadowPseudoId();
  if (!pseudo_id.empty()) {
    DCHECK(element.IsStyledElement());
    for (const auto bundle : match_request.AllRuleSets()) {
      if (CollectMatchingRulesForList<stop_at_first_match>(
              bundle.rule_set->UAShadowPseudoElementRules(pseudo_id),
              match_request, bundle.rule_set, bundle.style_sheet_index, checker,
              context.context) &&
          stop_at_first_match) {
        return true;
      }
    }
  }

  if (element.IsVTTElement()) {
    for (const auto bundle : match_request.AllRuleSets()) {
      if (CollectMatchingRulesForList<stop_at_first_match>(
              bundle.rule_set->CuePseudoRules(), match_request, bundle.rule_set,
              bundle.style_sheet_index, checker, context.context) &&
          stop_at_first_match) {
        return true;
      }
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
    return false;
  }

  // We need to collect the rules for id, class, tag, and everything else into a
  // buffer and then sort the buffer.
  if (element.HasID()) {
    for (const auto bundle : match_request.AllRuleSets()) {
      if (CollectMatchingRulesForList<stop_at_first_match>(
              bundle.rule_set->IdRules(element.IdForStyleResolution()),
              match_request, bundle.rule_set, bundle.style_sheet_index, checker,
              context.context) &&
          stop_at_first_match) {
        return true;
      }
    }
  }
  if (element.IsStyledElement() && element.HasClass()) {
    for (const AtomicString& class_name : element.ClassNames()) {
      for (const auto bundle : match_request.AllRuleSets()) {
        if (CollectMatchingRulesForList<stop_at_first_match>(
                bundle.rule_set->ClassRules(class_name), match_request,
                bundle.rule_set, bundle.style_sheet_index, checker,
                context.context) &&
            stop_at_first_match) {
          return true;
        }
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
        if (!bundle.rule_set->HasAnyAttrRules()) {
          continue;
        }
        base::span<const RuleData> list =
            bundle.rule_set->AttrRules(lower_name);
        if (list.empty() ||
            bundle.rule_set->CanIgnoreEntireList(
                list, lower_name, attributes[attr_idx].Value())) {
          continue;
        }
        if (CollectMatchingRulesForList<stop_at_first_match>(
                bundle.rule_set->AttrRules(lower_name), match_request,
                bundle.rule_set, bundle.style_sheet_index, checker,
                context.context) &&
            stop_at_first_match) {
          return true;
        }
      }

      const AttributeCollection collection = element.AttributesWithoutUpdate();
      attributes = {collection.data(), collection.size()};
    }
  }

  if (element.IsLink()) {
    for (const auto bundle : match_request.AllRuleSets()) {
      if (CollectMatchingRulesForList<stop_at_first_match>(
              bundle.rule_set->LinkPseudoClassRules(), match_request,
              bundle.rule_set, bundle.style_sheet_index, checker,
              context.context) &&
          stop_at_first_match) {
        return true;
      }
    }
  }
  if (SelectorChecker::MatchesFocusPseudoClass(element, kPseudoIdNone)) {
    for (const auto bundle : match_request.AllRuleSets()) {
      if (CollectMatchingRulesForList<stop_at_first_match>(
              bundle.rule_set->FocusPseudoClassRules(), match_request,
              bundle.rule_set, bundle.style_sheet_index, checker,
              context.context) &&
          stop_at_first_match) {
        return true;
      }
    }
  }
  if (SelectorChecker::MatchesSelectorFragmentAnchorPseudoClass(element)) {
    for (const auto bundle : match_request.AllRuleSets()) {
      if (CollectMatchingRulesForList<stop_at_first_match>(
              bundle.rule_set->SelectorFragmentAnchorRules(), match_request,
              bundle.rule_set, bundle.style_sheet_index, checker,
              context.context) &&
          stop_at_first_match) {
        return true;
      }
    }
  }
  if (SelectorChecker::MatchesFocusVisiblePseudoClass(element)) {
    for (const auto bundle : match_request.AllRuleSets()) {
      if (CollectMatchingRulesForList<stop_at_first_match>(
              bundle.rule_set->FocusVisiblePseudoClassRules(), match_request,
              bundle.rule_set, bundle.style_sheet_index, checker,
              context.context) &&
          stop_at_first_match) {
        return true;
      }
    }
  }
  if (element.GetDocument().documentElement() == element) {
    for (const auto bundle : match_request.AllRuleSets()) {
      if (CollectMatchingRulesForList<stop_at_first_match>(
              bundle.rule_set->RootElementRules(), match_request,
              bundle.rule_set, bundle.style_sheet_index, checker,
              context.context) &&
          stop_at_first_match) {
        return true;
      }
    }
  }
  AtomicString element_name = matching_ua_rules_
                                  ? element.localName()
                                  : element.LocalNameForSelectorMatching();
  for (const auto bundle : match_request.AllRuleSets()) {
    if (CollectMatchingRulesForList<stop_at_first_match>(
            bundle.rule_set->TagRules(element_name), match_request,
            bundle.rule_set, bundle.style_sheet_index, checker,
            context.context) &&
        stop_at_first_match) {
      return true;
    }
  }
  for (const auto bundle : match_request.AllRuleSets()) {
    if (CollectMatchingRulesForList<stop_at_first_match>(
            bundle.rule_set->UniversalRules(), match_request, bundle.rule_set,
            bundle.style_sheet_index, checker, context.context) &&
        stop_at_first_match) {
      return true;
    }
  }
  return false;
}

void ElementRuleCollector::CollectMatchingShadowHostRules(
    const MatchRequest& match_request) {
  SelectorChecker checker(nullptr, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  ContextWithStyleScopeFrame context(&context_.GetElement(), match_request,
                                     &pseudo_style_request_,
                                     style_recalc_context_.style_scope_frame);

  for (const auto bundle : match_request.AllRuleSets()) {
    CollectMatchingRulesForList</*stop_at_first_match=*/false>(
        bundle.rule_set->ShadowHostRules(), match_request, bundle.rule_set,
        bundle.style_sheet_index, checker, context.context);
    if (bundle.rule_set->MustCheckUniversalBucketForShadowHost()) {
      CollectMatchingRulesForList</*stop_at_first_match=*/false>(
          bundle.rule_set->UniversalRules(), match_request, bundle.rule_set,
          bundle.style_sheet_index, checker, context.context);
    }
  }
}

bool ElementRuleCollector::CheckIfAnyShadowHostRuleMatches(
    const MatchRequest& match_request) {
  SelectorChecker checker(nullptr, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  ContextWithStyleScopeFrame context(&context_.GetElement(), match_request,
                                     &pseudo_style_request_,
                                     style_recalc_context_.style_scope_frame);

  for (const auto bundle : match_request.AllRuleSets()) {
    if (CollectMatchingRulesForList</*stop_at_first_match=*/true>(
            bundle.rule_set->ShadowHostRules(), match_request, bundle.rule_set,
            bundle.style_sheet_index, checker, context.context)) {
      return true;
    }
    if (bundle.rule_set->MustCheckUniversalBucketForShadowHost()) {
      if (CollectMatchingRulesForList</*stop_at_first_match=*/true>(
              bundle.rule_set->UniversalRules(), match_request, bundle.rule_set,
              bundle.style_sheet_index, checker, context.context)) {
        return true;
      }
    }
  }
  return false;
}

void ElementRuleCollector::CollectMatchingSlottedRules(
    const MatchRequest& match_request) {
  SelectorChecker checker(nullptr, pseudo_style_request_, mode_,
                          matching_ua_rules_);
  ContextWithStyleScopeFrame context(&context_.GetElement(), match_request,
                                     &pseudo_style_request_,
                                     style_recalc_context_.style_scope_frame);

  for (const auto bundle : match_request.AllRuleSets()) {
    CollectMatchingRulesForList</*stop_at_first_match=*/false>(
        bundle.rule_set->SlottedPseudoElementRules(), match_request,
        bundle.rule_set, bundle.style_sheet_index, checker, context.context);
  }
}

void ElementRuleCollector::CollectMatchingPartPseudoRules(
    const MatchRequest& match_request,
    PartNames* part_names,
    bool for_shadow_pseudo) {
  PartRequest request{for_shadow_pseudo};
  SelectorChecker checker(part_names, pseudo_style_request_, mode_,
                          matching_ua_rules_);

  ContextWithStyleScopeFrame context(&context_.GetElement(), match_request,
                                     &pseudo_style_request_,
                                     style_recalc_context_.style_scope_frame);

  for (const auto bundle : match_request.AllRuleSets()) {
    CollectMatchingRulesForList</*stop_at_first_match=*/false>(
        bundle.rule_set->PartPseudoRules(), match_request, bundle.rule_set,
        bundle.style_sheet_index, checker, context.context, &request);
  }
}

// Find the CSSRule within the CSSRuleCollection that corresponds to the
// incoming StyleRule. This mapping is needed because Inspector needs to
// interact with the CSSOM-wrappers (i.e. CSSRules) of the matched rules, but
// ElementRuleCollector's result is a list of StyleRules.
//
// We also use it as a simple true/false for whether the StyleRule exists
// in the given style sheet, because we don't track which style sheet
// each matched rule came from in normal operation.
template <class CSSRuleCollection>
static CSSRule* FindStyleRule(CSSRuleCollection* css_rules,
                              const StyleRule* style_rule) {
  if (!css_rules) {
    return nullptr;
  }

  for (unsigned i = 0; i < css_rules->length(); ++i) {
    CSSRule* css_rule = css_rules->ItemInternal(i);
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
    } else if (auto* nested_declarations =
                   DynamicTo<CSSNestedDeclarationsRule>(css_rule)) {
      if (nested_declarations->NestedDeclarationsRule()->InnerStyleRule() ==
          style_rule) {
        return nested_declarations->InnerCSSStyleRule();
      }
    }
  }
  return nullptr;
}

void ElementRuleCollector::AppendCSSOMWrapperForRule(
    const TreeScope* tree_scope_containing_rule,
    const RuleData* rule_data,
    wtf_size_t position) {
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
  if (tree_scope_containing_rule) {
    for (const auto& [parent_style_sheet, rule_set] :
         tree_scope_containing_rule->GetScopedStyleResolver()
             ->GetActiveStyleSheets()) {
      css_rule = FindStyleRule(parent_style_sheet.Get(), rule);
      if (css_rule) {
        break;
      }
    }
    DCHECK(css_rule);
  } else {
    // |tree_scope_containing_rule| is nullptr if and only if the |rule| is
    // coming from User Agent. In this case, it is safe to create CSSOM wrappers
    // without parentStyleSheets as they will be used only by inspector which
    // will not try to edit them.
    css_rule = rule->CreateCSSOMWrapper(position);
  }
  EnsureRuleList()->emplace_back(css_rule, rule_data->SelectorIndex());
}

void ElementRuleCollector::SortAndTransferMatchedRules(
    CascadeOrigin origin,
    bool is_vtt_embedded_style,
    StyleRuleUsageTracker* tracker) {
  if (matched_rules_.empty()) {
    return;
  }

  SortMatchedRules();

  if (mode_ == SelectorChecker::kCollectingStyleRules) {
    for (const MatchedRule& matched_rule : matched_rules_) {
      EnsureStyleRuleList()->push_back(matched_rule.GetRuleData()->Rule());
    }
    return;
  }

  if (mode_ == SelectorChecker::kCollectingCSSRules) {
    for (unsigned i = 0; i < matched_rules_.size(); ++i) {
      AppendCSSOMWrapperForRule(current_matching_tree_scope_,
                                matched_rules_[i].GetRuleData(), i);
    }
    return;
  }

  // Now transfer the set of matched rules over to our list of declarations.
  for (const MatchedRule& matched_rule : matched_rules_) {
    const RuleData* rule_data = matched_rule.GetRuleData();
    if (rule_data->IsStartingStyle()) {
      result_.AddFlags(
          static_cast<MatchFlags>(MatchFlag::kAffectedByStartingStyle));
    }
    result_.AddMatchedProperties(
        &rule_data->Rule()->Properties(),
        {.link_match_type = static_cast<uint8_t>(
             AdjustLinkMatchType(inside_link_, rule_data->LinkMatchType())),
         .valid_property_filter = static_cast<uint8_t>(
             rule_data->GetValidPropertyFilter(matching_ua_rules_)),
         .is_inline_style = static_cast<uint8_t>(is_vtt_embedded_style),
         .origin = origin,
         .layer_order = matched_rule.LayerOrder()});
  }

  if (tracker) {
    AddMatchedRulesToTracker(tracker);
  }
}

void ElementRuleCollector::DidMatchRule(
    const RuleData* rule_data,
    uint16_t layer_order,
    const ContainerQuery* container_query,
    unsigned proximity,
    const SelectorChecker::MatchResult& result,
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
        universal = selector.IsLastInComplexSelector();
      } else if (const CSSSelector* next = selector.NextSimpleSelector()) {
        // When there is a default @namespace, ::selection and *::selection (not
        // universal) are stored as g_null_atom|*::selection, |*::selection (not
        // universal) is stored as g_empty_atom|*::selection, and *|*::selection
        // (the only universal form) is stored as g_star_atom|*::selection.
        universal =
            next->IsLastInComplexSelector() &&
            CSSSelector::GetPseudoId(next->GetPseudoType()) == dynamic_pseudo &&
            selector.Match() == CSSSelector::kTag &&
            selector.TagQName().LocalName().IsNull() &&
            selector.TagQName().Prefix() == g_star_atom;
      }

      if (!universal || container_query != nullptr) {
        result_.SetHasNonUniversalHighlightPseudoStyles();
      }

      if (!matching_ua_rules_) {
        result_.SetHasNonUaHighlightPseudoStyles();
      }

      if (container_query) {
        result_.SetHighlightsDependOnSizeContainerQueries();
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
    if (rule_data->Rule()->Properties().ContainsCursorHand()) {
      context_.GetElement().GetDocument().CountUse(
          WebFeature::kQuirksModeCursorHandApplied);
    }
    matched_rules_.emplace_back(rule_data, layer_order, proximity,
                                style_sheet_index);
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
            item_dict.Add("selector", it.key.selector);
            item_dict.Add("style_sheet_id", it.key.style_sheet_id);
            item_dict.Add("elapsed (us)", it.value.elapsed);
            item_dict.Add("match_attempts", it.value.match_attempts);
            item_dict.Add("fast_reject_count", it.value.fast_reject_count);
            item_dict.Add("match_count", it.value.match_count);
          }
        }
      });
  GetSelectorStatisticsRuleMap().clear();
}

struct ElementRuleCollector::CompareRules {
  inline bool operator()(const MatchedRule& matched_rule1,
                         const MatchedRule& matched_rule2) const {
#ifdef __SIZEOF_INT128__
    // https://github.com/llvm/llvm-project/issues/108418
    __uint128_t key1 = (__uint128_t{matched_rule1.SortKey()} << 64) |
                       matched_rule1.GetPosition();
    __uint128_t key2 = (__uint128_t{matched_rule2.SortKey()} << 64) |
                       matched_rule2.GetPosition();
#else
    std::pair key1{matched_rule1.SortKey(), matched_rule1.GetPosition()};
    std::pair key2{matched_rule2.SortKey(), matched_rule2.GetPosition()};
#endif
    return key1 < key2;
  }
};

void ElementRuleCollector::SortMatchedRules() {
  if (matched_rules_.size() > 1) {
    std::sort(matched_rules_.begin(), matched_rules_.end(), CompareRules());
  }
}

void ElementRuleCollector::AddMatchedRulesToTracker(
    StyleRuleUsageTracker* tracker) const {
  for (auto matched_rule : matched_rules_) {
    const StyleRule* rule = matched_rule.GetRuleData()->Rule();
    tracker->Track(
        FindStyleSheet(current_matching_tree_scope_,
                       context_.GetElement().GetDocument().GetStyleEngine(),
                       rule),
        rule);
  }
}

}  // namespace blink
