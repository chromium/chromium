/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
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

#include "third_party/blink/renderer/core/css/rule_set.h"

#include <memory>
#include <type_traits>
#include <vector>

#include "base/substring_set_matcher/substring_set_matcher.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

using base::MatcherStringPattern;
using base::SubstringSetMatcher;

namespace blink {

template <class T>
static void AddRuleToIntervals(const T* value,
                               unsigned position,
                               HeapVector<RuleSet::Interval<T>>& intervals);

static inline ValidPropertyFilter DetermineValidPropertyFilter(
    const AddRuleFlags add_rule_flags,
    const CSSSelector& selector) {
  for (const CSSSelector* component = &selector; component;
       component = component->TagHistory()) {
    if (component->Match() == CSSSelector::kPseudoElement &&
        component->Value() == TextTrackCue::CueShadowPseudoId()) {
      return ValidPropertyFilter::kCue;
    }
    switch (component->GetPseudoType()) {
      case CSSSelector::kPseudoCue:
        return ValidPropertyFilter::kCue;
      case CSSSelector::kPseudoFirstLetter:
        return ValidPropertyFilter::kFirstLetter;
      case CSSSelector::kPseudoFirstLine:
        return ValidPropertyFilter::kFirstLine;
      case CSSSelector::kPseudoMarker:
        return ValidPropertyFilter::kMarker;
      case CSSSelector::kPseudoSelection:
      case CSSSelector::kPseudoTargetText:
      case CSSSelector::kPseudoGrammarError:
      case CSSSelector::kPseudoSpellingError:
        if (RuntimeEnabledFeatures::HighlightInheritanceEnabled())
          return ValidPropertyFilter::kHighlight;
        else
          return ValidPropertyFilter::kHighlightLegacy;
      case CSSSelector::kPseudoHighlight:
        return ValidPropertyFilter::kHighlight;
      default:
        break;
    }
  }
  return ValidPropertyFilter::kNoFilter;
}

static unsigned DetermineLinkMatchType(const AddRuleFlags add_rule_flags,
                                       const CSSSelector& selector) {
  if (selector.HasLinkOrVisited()) {
    return (add_rule_flags & kRuleIsVisitedDependent)
               ? CSSSelector::kMatchVisited
               : CSSSelector::kMatchLink;
  }
  return CSSSelector::kMatchAll;
}

RuleData::RuleData(StyleRule* rule,
                   unsigned selector_index,
                   unsigned position,
                   unsigned extra_specificity,
                   AddRuleFlags add_rule_flags)
    : rule_(rule),
      selector_index_(selector_index),
      position_(position),
      specificity_(Selector().Specificity() + extra_specificity),
      link_match_type_(DetermineLinkMatchType(add_rule_flags, Selector())),
      has_document_security_origin_(add_rule_flags &
                                    kRuleHasDocumentSecurityOrigin),
      valid_property_filter_(
          static_cast<std::underlying_type_t<ValidPropertyFilter>>(
              DetermineValidPropertyFilter(add_rule_flags, Selector()))),
      descendant_selector_identifier_hashes_() {}

void RuleData::ComputeBloomFilterHashes() {
#if DCHECK_IS_ON()
  marker_ = 0;
#endif
  SelectorFilter::CollectIdentifierHashes(
      Selector(), descendant_selector_identifier_hashes_,
      kMaximumIdentifierCount);
}

void RuleSet::AddToRuleSet(const AtomicString& key,
                           RuleMap& map,
                           const RuleData& rule_data) {
  if (map.IsCompacted()) {
    // This normally should not happen, but may with UA stylesheets;
    // see class comment on RuleMap.
    map.Uncompact();
  }
  map.Add(key, rule_data);
  // Don't call ComputeBloomFilterHashes() here; RuleMap needs that space for
  // group information, and will call ComputeBloomFilterHashes() itself on
  // compaction.
  need_compaction_ = true;
}

void RuleSet::AddToRuleSet(HeapVector<RuleData>& rules,
                           const RuleData& rule_data) {
  rules.push_back(rule_data);
  rules.back().ComputeBloomFilterHashes();
  need_compaction_ = true;
}

static void ExtractSelectorValues(const CSSSelector* selector,
                                  AtomicString& id,
                                  AtomicString& class_name,
                                  AtomicString& attr_name,
                                  AtomicString& attr_value,
                                  bool& is_exact_attr,
                                  AtomicString& custom_pseudo_element_name,
                                  AtomicString& tag_name,
                                  AtomicString& part_name,
                                  CSSSelector::PseudoType& pseudo_type) {
  is_exact_attr = false;
  switch (selector->Match()) {
    case CSSSelector::kId:
      id = selector->Value();
      break;
    case CSSSelector::kClass:
      class_name = selector->Value();
      break;
    case CSSSelector::kTag:
      if (selector->TagQName().LocalName() !=
          CSSSelector::UniversalSelectorAtom())
        tag_name = selector->TagQName().LocalName();
      break;
    case CSSSelector::kPseudoClass:
    case CSSSelector::kPseudoElement:
    case CSSSelector::kPagePseudoClass:
      // Must match the cases in RuleSet::FindBestRuleSetAndAdd.
      switch (selector->GetPseudoType()) {
        case CSSSelector::kPseudoCue:
        case CSSSelector::kPseudoLink:
        case CSSSelector::kPseudoVisited:
        case CSSSelector::kPseudoWebkitAnyLink:
        case CSSSelector::kPseudoAnyLink:
        case CSSSelector::kPseudoFocus:
        case CSSSelector::kPseudoFocusVisible:
        case CSSSelector::kPseudoPlaceholder:
        case CSSSelector::kPseudoFileSelectorButton:
        case CSSSelector::kPseudoHost:
        case CSSSelector::kPseudoHostContext:
        case CSSSelector::kPseudoSpatialNavigationInterest:
        case CSSSelector::kPseudoSlotted:
        case CSSSelector::kPseudoSelectorFragmentAnchor:
          pseudo_type = selector->GetPseudoType();
          break;
        case CSSSelector::kPseudoWebKitCustomElement:
        case CSSSelector::kPseudoBlinkInternalElement:
          custom_pseudo_element_name = selector->Value();
          break;
        case CSSSelector::kPseudoPart:
          part_name = selector->Value();
          break;
        case CSSSelector::kPseudoIs:
        case CSSSelector::kPseudoWhere: {
          const CSSSelectorList* selector_list = selector->SelectorList();
          DCHECK(selector_list);
          // If the :is/:where has only a single argument, it effectively acts
          // like a normal selector (save for specificity), and we can put it
          // into a bucket based on that selector.
          if (selector_list->HasOneSelector()) {
            ExtractSelectorValues(selector_list->First(), id, class_name,
                                  attr_name, attr_value, is_exact_attr,
                                  custom_pseudo_element_name, tag_name,
                                  part_name, pseudo_type);
          }
        } break;
        default:
          break;
      }
      break;
    case CSSSelector::kAttributeExact:
      is_exact_attr = true;
      [[fallthrough]];
    case CSSSelector::kAttributeSet:
    case CSSSelector::kAttributeHyphen:
    case CSSSelector::kAttributeList:
    case CSSSelector::kAttributeContain:
    case CSSSelector::kAttributeBegin:
    case CSSSelector::kAttributeEnd:
      attr_name = selector->Attribute().LocalName();
      attr_value = selector->Value();
      break;
    default:
      break;
  }
}

// For a (possibly compound) selector, extract the values used for determining
// its buckets (e.g. for “.foo[baz]”, will return foo for class_name and
// baz for attr_name). Returns the last subselector in the group, which is also
// the one given the highest priority.
static const CSSSelector* ExtractBestSelectorValues(
    const CSSSelector& component,
    AtomicString& id,
    AtomicString& class_name,
    AtomicString& attr_name,
    AtomicString& attr_value,
    bool& is_exact_attr,
    AtomicString& custom_pseudo_element_name,
    AtomicString& tag_name,
    AtomicString& part_name,
    CSSSelector::PseudoType& pseudo_type) {
  const CSSSelector* it = &component;
  for (; it && it->Relation() == CSSSelector::kSubSelector;
       it = it->TagHistory()) {
    ExtractSelectorValues(it, id, class_name, attr_name, attr_value,
                          is_exact_attr, custom_pseudo_element_name, tag_name,
                          part_name, pseudo_type);
  }
  if (it) {
    ExtractSelectorValues(it, id, class_name, attr_name, attr_value,
                          is_exact_attr, custom_pseudo_element_name, tag_name,
                          part_name, pseudo_type);
  }
  return it;
}

bool RuleSet::FindBestRuleSetAndAdd(const CSSSelector& component,
                                    const RuleData& rule_data) {
  AtomicString id;
  AtomicString class_name;
  AtomicString attr_name;
  AtomicString attr_value;  // Unused.
  AtomicString custom_pseudo_element_name;
  AtomicString tag_name;
  AtomicString part_name;
  CSSSelector::PseudoType pseudo_type = CSSSelector::kPseudoUnknown;

#ifndef NDEBUG
  all_rules_.push_back(rule_data);
#endif

  bool is_exact_attr;
  const CSSSelector* it = ExtractBestSelectorValues(
      component, id, class_name, attr_name, attr_value, is_exact_attr,
      custom_pseudo_element_name, tag_name, part_name, pseudo_type);

  // Prefer rule sets in order of most likely to apply infrequently.
  if (!id.empty()) {
    AddToRuleSet(id, id_rules_, rule_data);
    return true;
  }

  if (!class_name.empty()) {
    AddToRuleSet(class_name, class_rules_, rule_data);
    return true;
  }

  if (!attr_name.empty()) {
    AddToRuleSet(attr_name, attr_rules_, rule_data);
    if (attr_name == html_names::kStyleAttr) {
      has_bucket_for_style_attr_ = true;
    }
    return true;
  }

  if (!custom_pseudo_element_name.empty()) {
    // Custom pseudos come before ids and classes in the order of tagHistory,
    // and have a relation of ShadowPseudo between them. Therefore we should
    // never be a situation where ExtractSelectorValues finds id and
    // className in addition to custom pseudo.
    DCHECK(id.empty());
    DCHECK(class_name.empty());
    AddToRuleSet(custom_pseudo_element_name, ua_shadow_pseudo_element_rules_,
                 rule_data);
    return true;
  }

  if (!part_name.empty()) {
    AddToRuleSet(part_pseudo_rules_, rule_data);
    return true;
  }

  switch (pseudo_type) {
    case CSSSelector::kPseudoCue:
      AddToRuleSet(cue_pseudo_rules_, rule_data);
      return true;
    case CSSSelector::kPseudoLink:
    case CSSSelector::kPseudoVisited:
    case CSSSelector::kPseudoAnyLink:
    case CSSSelector::kPseudoWebkitAnyLink:
      AddToRuleSet(link_pseudo_class_rules_, rule_data);
      return true;
    case CSSSelector::kPseudoSpatialNavigationInterest:
      AddToRuleSet(spatial_navigation_interest_class_rules_, rule_data);
      return true;
    case CSSSelector::kPseudoFocus:
      AddToRuleSet(focus_pseudo_class_rules_, rule_data);
      return true;
    case CSSSelector::kPseudoSelectorFragmentAnchor:
      AddToRuleSet(selector_fragment_anchor_rules_, rule_data);
      return true;
    case CSSSelector::kPseudoFocusVisible:
      AddToRuleSet(focus_visible_pseudo_class_rules_, rule_data);
      return true;
    case CSSSelector::kPseudoPlaceholder:
    case CSSSelector::kPseudoFileSelectorButton:
      if (it->FollowsPart()) {
        AddToRuleSet(part_pseudo_rules_, rule_data);
      } else if (it->FollowsSlotted()) {
        AddToRuleSet(slotted_pseudo_element_rules_, rule_data);
      } else {
        const auto& name = pseudo_type == CSSSelector::kPseudoFileSelectorButton
                               ? shadow_element_names::kPseudoFileUploadButton
                               : shadow_element_names::kPseudoInputPlaceholder;
        AddToRuleSet(name, ua_shadow_pseudo_element_rules_, rule_data);
      }
      return true;
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoHostContext:
      AddToRuleSet(shadow_host_rules_, rule_data);
      return true;
    case CSSSelector::kPseudoSlotted:
      AddToRuleSet(slotted_pseudo_element_rules_, rule_data);
      return true;
    default:
      break;
  }

  if (!tag_name.empty()) {
    AddToRuleSet(tag_name, tag_rules_, rule_data);
    return true;
  }

  return false;
}

void RuleSet::AddRule(StyleRule* rule,
                      unsigned selector_index,
                      AddRuleFlags add_rule_flags,
                      const ContainerQuery* container_query,
                      const CascadeLayer* cascade_layer,
                      const StyleScope* style_scope) {
  // The selector index field in RuleData is only 13 bits so we can't support
  // selectors at index 8192 or beyond.
  // See https://crbug.com/804179
  if (selector_index >= (1 << RuleData::kSelectorIndexBits)) {
    return;
  }
  if (rule_count_ >= (1 << RuleData::kPositionBits)) {
    return;
  }
  const int extra_specificity = style_scope ? style_scope->Specificity() : 0;
  RuleData rule_data(rule, selector_index, rule_count_, extra_specificity,
                     add_rule_flags);
  ++rule_count_;
  if (features_.CollectFeaturesFromSelector(rule_data.Selector(),
                                            style_scope) ==
      RuleFeatureSet::kSelectorNeverMatches)
    return;

  if (!FindBestRuleSetAndAdd(rule_data.Selector(), rule_data)) {
    // If we didn't find a specialized map to stick it in, file under universal
    // rules.
    AddToRuleSet(universal_rules_, rule_data);
  }

  // If the rule has CSSSelector::kMatchLink, it means that there is a :visited
  // or :link pseudo-class somewhere in the selector. In those cases, we
  // effectively split the rule into two: one which covers the situation
  // where we are in an unvisited link (kMatchLink), and another which covers
  // the visited link case (kMatchVisited).
  if (rule_data.LinkMatchType() == CSSSelector::kMatchLink) {
    RuleData visited_dependent(rule, rule_data.SelectorIndex(),
                               rule_data.GetPosition(), extra_specificity,
                               add_rule_flags | kRuleIsVisitedDependent);
    AddToRuleSet(visited_dependent_rules_, visited_dependent);
  }

  AddRuleToLayerIntervals(cascade_layer, rule_data.GetPosition());
  AddRuleToIntervals(container_query, rule_data.GetPosition(),
                     container_query_intervals_);
  AddRuleToIntervals(style_scope, rule_data.GetPosition(), scope_intervals_);
}

void RuleSet::AddRuleToLayerIntervals(const CascadeLayer* cascade_layer,
                                      unsigned position) {
  // Add a new interval only if the current layer is different from the last
  // interval's layer. Note that the implicit outer layer may also be
  // represented by a nullptr.
  const CascadeLayer* last_interval_layer =
      layer_intervals_.empty() ? implicit_outer_layer_.Get()
                               : layer_intervals_.back().value.Get();
  if (!cascade_layer)
    cascade_layer = implicit_outer_layer_;
  if (cascade_layer == last_interval_layer)
    return;

  if (!cascade_layer)
    cascade_layer = EnsureImplicitOuterLayer();
  layer_intervals_.push_back(Interval<CascadeLayer>(cascade_layer, position));
}

// Similar to AddRuleToLayerIntervals, but for container queries and @style
// scopes.
template <class T>
static void AddRuleToIntervals(const T* value,
                               unsigned position,
                               HeapVector<RuleSet::Interval<T>>& intervals) {
  const T* last_value =
      intervals.empty() ? nullptr : intervals.back().value.Get();
  if (value == last_value)
    return;

  intervals.push_back(RuleSet::Interval<T>(value, position));
}

void RuleSet::AddPageRule(StyleRulePage* rule) {
  need_compaction_ = true;
  page_rules_.push_back(rule);
}

void RuleSet::AddFontFaceRule(StyleRuleFontFace* rule) {
  need_compaction_ = true;
  font_face_rules_.push_back(rule);
}

void RuleSet::AddKeyframesRule(StyleRuleKeyframes* rule) {
  need_compaction_ = true;
  keyframes_rules_.push_back(rule);
}

void RuleSet::AddPropertyRule(StyleRuleProperty* rule) {
  need_compaction_ = true;
  property_rules_.push_back(rule);
}

void RuleSet::AddCounterStyleRule(StyleRuleCounterStyle* rule) {
  need_compaction_ = true;
  counter_style_rules_.push_back(rule);
}

void RuleSet::AddFontPaletteValuesRule(StyleRuleFontPaletteValues* rule) {
  need_compaction_ = true;
  font_palette_values_rules_.push_back(rule);
}

void RuleSet::AddFontFeatureValuesRule(StyleRuleFontFeatureValues* rule) {
  need_compaction_ = true;
  font_feature_values_rules_.push_back(rule);
}

void RuleSet::AddPositionFallbackRule(StyleRulePositionFallback* rule) {
  need_compaction_ = true;
  position_fallback_rules_.push_back(rule);
}

void RuleSet::AddChildRules(const HeapVector<Member<StyleRuleBase>>& rules,
                            const MediaQueryEvaluator& medium,
                            AddRuleFlags add_rule_flags,
                            const ContainerQuery* container_query,
                            CascadeLayer* cascade_layer,
                            const StyleScope* style_scope) {
  for (unsigned i = 0; i < rules.size(); ++i) {
    StyleRuleBase* rule = rules[i].Get();

    if (auto* style_rule = DynamicTo<StyleRule>(rule)) {
      AddStyleRule(style_rule, medium, add_rule_flags, container_query,
                   cascade_layer, style_scope);
    } else if (auto* page_rule = DynamicTo<StyleRulePage>(rule)) {
      page_rule->SetCascadeLayer(cascade_layer);
      AddPageRule(page_rule);
    } else if (auto* media_rule = DynamicTo<StyleRuleMedia>(rule)) {
      if (MatchMediaForAddRules(medium, media_rule->MediaQueries())) {
        AddChildRules(media_rule->ChildRules(), medium, add_rule_flags,
                      container_query, cascade_layer, style_scope);
      }
    } else if (auto* font_face_rule = DynamicTo<StyleRuleFontFace>(rule)) {
      font_face_rule->SetCascadeLayer(cascade_layer);
      AddFontFaceRule(font_face_rule);
    } else if (auto* font_palette_values_rule =
                   DynamicTo<StyleRuleFontPaletteValues>(rule)) {
      // TODO(https://crbug.com/1170794): Handle cascade layers for
      // @font-palette-values.
      AddFontPaletteValuesRule(font_palette_values_rule);
    } else if (auto* font_feature_values_rule =
                   DynamicTo<StyleRuleFontFeatureValues>(rule)) {
      // TODO(crbug.com/1394327): Handle cascade layers for
      // @font-feature-values.
      AddFontFeatureValuesRule(font_feature_values_rule);
    } else if (auto* keyframes_rule = DynamicTo<StyleRuleKeyframes>(rule)) {
      keyframes_rule->SetCascadeLayer(cascade_layer);
      AddKeyframesRule(keyframes_rule);
    } else if (auto* property_rule = DynamicTo<StyleRuleProperty>(rule)) {
      property_rule->SetCascadeLayer(cascade_layer);
      AddPropertyRule(property_rule);
    } else if (auto* counter_style_rule =
                   DynamicTo<StyleRuleCounterStyle>(rule)) {
      counter_style_rule->SetCascadeLayer(cascade_layer);
      AddCounterStyleRule(counter_style_rule);
    } else if (auto* position_fallback_rule =
                   DynamicTo<StyleRulePositionFallback>(rule)) {
      position_fallback_rule->SetCascadeLayer(cascade_layer);
      AddPositionFallbackRule(position_fallback_rule);
    } else if (auto* supports_rule = DynamicTo<StyleRuleSupports>(rule)) {
      if (supports_rule->ConditionIsSupported()) {
        AddChildRules(supports_rule->ChildRules(), medium, add_rule_flags,
                      container_query, cascade_layer, style_scope);
      }
    } else if (auto* container_rule = DynamicTo<StyleRuleContainer>(rule)) {
      const ContainerQuery* inner_container_query =
          &container_rule->GetContainerQuery();
      if (container_query) {
        inner_container_query =
            inner_container_query->CopyWithParent(container_query);
      }
      AddChildRules(container_rule->ChildRules(), medium, add_rule_flags,
                    inner_container_query, cascade_layer, style_scope);
    } else if (auto* layer_block_rule = DynamicTo<StyleRuleLayerBlock>(rule)) {
      CascadeLayer* sub_layer =
          GetOrAddSubLayer(cascade_layer, layer_block_rule->GetName());
      AddChildRules(layer_block_rule->ChildRules(), medium, add_rule_flags,
                    container_query, sub_layer, style_scope);
    } else if (auto* layer_statement_rule =
                   DynamicTo<StyleRuleLayerStatement>(rule)) {
      for (const auto& layer_name : layer_statement_rule->GetNames())
        GetOrAddSubLayer(cascade_layer, layer_name);
    } else if (auto* scope_rule = DynamicTo<StyleRuleScope>(rule)) {
      const StyleScope* inner_style_scope = &scope_rule->GetStyleScope();
      if (style_scope)
        inner_style_scope = inner_style_scope->CopyWithParent(style_scope);
      AddChildRules(scope_rule->ChildRules(), medium, add_rule_flags,
                    container_query, cascade_layer, inner_style_scope);
    }
  }
}

bool RuleSet::MatchMediaForAddRules(const MediaQueryEvaluator& evaluator,
                                    const MediaQuerySet* media_queries) {
  if (!media_queries)
    return true;
  bool match_media =
      evaluator.Eval(*media_queries, &features_.MutableMediaQueryResultFlags());
  media_query_set_results_.push_back(
      MediaQuerySetResult(*media_queries, match_media));
  return match_media;
}

void RuleSet::AddRulesFromSheet(StyleSheetContents* sheet,
                                const MediaQueryEvaluator& medium,
                                AddRuleFlags add_rule_flags,
                                CascadeLayer* cascade_layer) {
  TRACE_EVENT0("blink", "RuleSet::addRulesFromSheet");

  DCHECK(sheet);

  for (const auto& pre_import_layer : sheet->PreImportLayerStatementRules()) {
    for (const auto& name : pre_import_layer->GetNames())
      GetOrAddSubLayer(cascade_layer, name);
  }

  const HeapVector<Member<StyleRuleImport>>& import_rules =
      sheet->ImportRules();
  for (unsigned i = 0; i < import_rules.size(); ++i) {
    StyleRuleImport* import_rule = import_rules[i].Get();
    if (!MatchMediaForAddRules(medium, import_rule->MediaQueries()))
      continue;
    CascadeLayer* import_layer = cascade_layer;
    if (import_rule->IsLayered()) {
      import_layer =
          GetOrAddSubLayer(cascade_layer, import_rule->GetLayerName());
    }
    if (import_rule->GetStyleSheet()) {
      AddRulesFromSheet(import_rule->GetStyleSheet(), medium, add_rule_flags,
                        import_layer);
    }
  }

  AddChildRules(sheet->ChildRules(), medium, add_rule_flags,
                nullptr /* container_query */, cascade_layer, nullptr);
}

void RuleSet::AddStyleRule(StyleRule* style_rule,
                           const MediaQueryEvaluator& medium,
                           AddRuleFlags add_rule_flags,
                           const ContainerQuery* container_query,
                           CascadeLayer* cascade_layer,
                           const StyleScope* style_scope) {
  for (const CSSSelector* selector = style_rule->FirstSelector(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    wtf_size_t selector_index = style_rule->SelectorIndex(*selector);
    AddRule(style_rule, selector_index, add_rule_flags, container_query,
            cascade_layer, style_scope);
  }

  // Nested rules are taken to be added immediately after their parent rule.
  if (style_rule->ChildRules() != nullptr) {
    AddChildRules(*style_rule->ChildRules(), medium, add_rule_flags,
                  container_query, cascade_layer, style_scope);
  }
}

CascadeLayer* RuleSet::GetOrAddSubLayer(CascadeLayer* cascade_layer,
                                        const StyleRuleBase::LayerName& name) {
  if (!cascade_layer)
    cascade_layer = EnsureImplicitOuterLayer();
  return cascade_layer->GetOrAddSubLayer(name);
}

void RuleMap::Add(const AtomicString& key, const RuleData& rule_data) {
  RuleMap::Extent& rules =
      buckets.insert(key, RuleMap::Extent()).stored_value->value;
  if (rules.length == 0)
    rules.bucket_number = num_buckets++;
  RuleData rule_data_copy = rule_data;
  rule_data_copy.SetBucketInformation(rules.bucket_number,
                                      /*order_in_bucket=*/rules.length++);
  backing.push_back(std::move(rule_data_copy));
}

void RuleMap::Compact() {
  if (compacted) {
    return;
  }
  if (backing.empty()) {
    // Nothing to do.
    compacted = true;
    return;
  }

  backing.shrink_to_fit();

  // Order by (bucket_number, order_in_bucket) by way of a simple
  // in-place counting sort (which is O(n), because our highest bucket
  // number is always less than or equal to the number of elements).
  // First, we make an array that contains the number of elements in each
  // bucket, indexed by the bucket number.
  std::unique_ptr<unsigned[]> counts(new unsigned[num_buckets]());
  for (const RuleData& rule_data : backing) {
    ++counts[rule_data.GetBucketNumber()];
  }

  // Do the prefix sum. After this, counts[i] is the desired start index
  // for the i-th bucket.
  unsigned sum = 0;
  for (wtf_size_t i = 0; i < num_buckets; ++i) {
    DCHECK_GT(counts[i], 0U);
    unsigned new_sum = sum + counts[i];
    counts[i] = sum;
    sum = new_sum;
  }

  // Store that information into each bucket.
  for (auto& [key, value] : buckets) {
    value.start_index = counts[value.bucket_number];
  }

  // Now put each element into its right place. Every iteration, we will
  // either swap an element into its final destination, or, when we
  // encounter one that is already in its correct place (possibly
  // because we put it there earlier), skip to the next array slot.
  // These will happen exactly n times each, giving us our O(n) runtime.
  for (wtf_size_t i = 0; i < backing.size();) {
    const RuleData& rule_data = backing[i];
    wtf_size_t correct_pos =
        counts[rule_data.GetBucketNumber()] + rule_data.GetOrderInBucket();
    if (i == correct_pos) {
      ++i;
    } else {
      using std::swap;
      swap(backing[i], backing[correct_pos]);
    }
  }

  // Now that we don't need the grouping information anymore, we can compute
  // the Bloom filter hashes that want to stay in the same memory area.
  for (RuleData& rule_data : backing) {
    rule_data.ComputeBloomFilterHashes();
  }

  compacted = true;
}

void RuleMap::Uncompact() {
  num_buckets = 0;
  for (auto& [key, value] : buckets) {
    unsigned i = 0;
    for (RuleData& rule_data : GetRulesFromExtent(value)) {
      rule_data.SetBucketInformation(/*bucket_number=*/num_buckets,
                                     /*order_in_bucket=*/i++);
    }
    value.bucket_number = num_buckets++;
    value.length = i;
  }
  compacted = false;
}

static wtf_size_t GetMinimumRulesetSizeForSubstringMatcher() {
  // It's not worth going through the Aho-Corasick matcher unless we can
  // reject a reasonable number of rules in one go. Practical ad-hoc testing
  // suggests the break-even point between using the tree and just testing
  // all of the rules individually lies somewhere around 20–40 rules
  // (depending a bit on e.g. how hot the tree is in the cache, the length
  // of the value that we match against, and of course whether we actually
  // have a match). We add a little bit of margin to compensate for the fact
  // that we also need to spend time building the tree, and the extra memory
  // in use.
  return 50;
}

bool RuleSet::CanIgnoreEntireList(base::span<const RuleData> list,
                                  const AtomicString& key,
                                  const AtomicString& value) const {
  DCHECK_EQ(attr_rules_.Find(key).size(), list.size());
  if (!list.empty()) {
    DCHECK_EQ(attr_rules_.Find(key).data(), list.data());
  }
  if (list.size() < GetMinimumRulesetSizeForSubstringMatcher()) {
    // Too small to build up a tree, so always check.
    DCHECK_EQ(attr_substring_matchers_.find(key),
              attr_substring_matchers_.end());
    return false;
  }

  // See CreateSubstringMatchers().
  if (value.empty()) {
    return false;
  }

  auto it = attr_substring_matchers_.find(key);
  if (it == attr_substring_matchers_.end()) {
    // Building the tree failed, so always check.
    return false;
  }
  return !it->value->AnyMatch(value.LowerASCII().Utf8());
}

void RuleSet::CreateSubstringMatchers(
    RuleMap& attr_map,
    RuleSet::SubstringMatcherMap& substring_matcher_map) {
  for (const auto& [/*AtomicString*/ attr,
                    /*base::span<const RuleData>*/ ruleset] : attr_map) {
    if (ruleset.size() < GetMinimumRulesetSizeForSubstringMatcher()) {
      continue;
    }
    std::vector<MatcherStringPattern> patterns;
    int rule_index = 0;
    for (const RuleData& rule : ruleset) {
      AtomicString id;
      AtomicString class_name;
      AtomicString attr_name;
      AtomicString attr_value;
      AtomicString custom_pseudo_element_name;
      AtomicString tag_name;
      AtomicString part_name;
      bool is_exact_attr;
      CSSSelector::PseudoType pseudo_type = CSSSelector::kPseudoUnknown;
      ExtractBestSelectorValues(
          rule.Selector(), id, class_name, attr_name, attr_value, is_exact_attr,
          custom_pseudo_element_name, tag_name, part_name, pseudo_type);
      DCHECK(!attr_name.empty());

      if (attr_value.empty()) {
        if (is_exact_attr) {
          // The empty string would make the entire tree useless
          // (it is a substring of every possible value),
          // so as a special case, we ignore it, and have a separate
          // check in CanIgnoreEntireList().
          continue;
        } else {
          // This rule would indeed match every element containing the
          // given attribute (e.g. [foo] or [foo^=""]), so building a tree
          // would be wrong.
          patterns.clear();
          break;
        }
      }

      std::string pattern = attr_value.LowerASCII().Utf8();

      // SubstringSetMatcher doesn't like duplicates, and since we only
      // use the tree for true/false information anyway, we can remove them.
      bool already_exists =
          any_of(patterns.begin(), patterns.end(),
                 [&pattern](const MatcherStringPattern& existing_pattern) {
                   return existing_pattern.pattern() == pattern;
                 });
      if (!already_exists) {
        patterns.emplace_back(pattern, rule_index);
      }
      ++rule_index;
    }

    if (patterns.empty()) {
      continue;
    }

    auto substring_matcher = std::make_unique<SubstringSetMatcher>();
    if (!substring_matcher->Build(patterns)) {
      // Should never really happen unless there are megabytes and megabytes
      // of such classes, so we just drop out to the slow path.
    } else {
      substring_matcher_map.insert(attr, std::move(substring_matcher));
    }
  }
}

void RuleSet::CompactRules() {
  DCHECK(need_compaction_);
  id_rules_.Compact();
  class_rules_.Compact();
  attr_rules_.Compact();
  CreateSubstringMatchers(attr_rules_, attr_substring_matchers_);
  tag_rules_.Compact();
  ua_shadow_pseudo_element_rules_.Compact();
  link_pseudo_class_rules_.shrink_to_fit();
  cue_pseudo_rules_.shrink_to_fit();
  focus_pseudo_class_rules_.shrink_to_fit();
  selector_fragment_anchor_rules_.shrink_to_fit();
  focus_visible_pseudo_class_rules_.shrink_to_fit();
  spatial_navigation_interest_class_rules_.shrink_to_fit();
  universal_rules_.shrink_to_fit();
  shadow_host_rules_.shrink_to_fit();
  part_pseudo_rules_.shrink_to_fit();
  slotted_pseudo_element_rules_.shrink_to_fit();
  visited_dependent_rules_.shrink_to_fit();
  page_rules_.shrink_to_fit();
  font_face_rules_.shrink_to_fit();
  font_palette_values_rules_.shrink_to_fit();
  keyframes_rules_.shrink_to_fit();
  property_rules_.shrink_to_fit();
  counter_style_rules_.shrink_to_fit();
  position_fallback_rules_.shrink_to_fit();
  layer_intervals_.shrink_to_fit();

#if EXPENSIVE_DCHECKS_ARE_ON()
  AssertRuleListsSorted();
#endif
  need_compaction_ = false;
}

#if EXPENSIVE_DCHECKS_ARE_ON()

namespace {

template <class RuleList>
bool IsRuleListSorted(const RuleList& rules) {
  unsigned last_position = 0;
  bool first_rule = true;
  for (const RuleData& rule : rules) {
    if (!first_rule && rule.GetPosition() <= last_position)
      return false;
    first_rule = false;
    last_position = rule.GetPosition();
  }
  return true;
}

}  // namespace

void RuleSet::AssertRuleListsSorted() const {
  for (const auto& item : id_rules_) {
    DCHECK(IsRuleListSorted(item.value));
  }
  for (const auto& item : class_rules_) {
    DCHECK(IsRuleListSorted(item.value));
  }
  for (const auto& item : tag_rules_) {
    DCHECK(IsRuleListSorted(item.value));
  }
  for (const auto& item : ua_shadow_pseudo_element_rules_) {
    DCHECK(IsRuleListSorted(item.value));
  }
  DCHECK(IsRuleListSorted(link_pseudo_class_rules_));
  DCHECK(IsRuleListSorted(cue_pseudo_rules_));
  DCHECK(IsRuleListSorted(focus_pseudo_class_rules_));
  DCHECK(IsRuleListSorted(selector_fragment_anchor_rules_));
  DCHECK(IsRuleListSorted(focus_visible_pseudo_class_rules_));
  DCHECK(IsRuleListSorted(spatial_navigation_interest_class_rules_));
  DCHECK(IsRuleListSorted(universal_rules_));
  DCHECK(IsRuleListSorted(shadow_host_rules_));
  DCHECK(IsRuleListSorted(part_pseudo_rules_));
  DCHECK(IsRuleListSorted(visited_dependent_rules_));
}

#endif  // EXPENSIVE_DCHECKS_ARE_ON()

bool RuleSet::DidMediaQueryResultsChange(
    const MediaQueryEvaluator& evaluator) const {
  return evaluator.DidResultsChange(media_query_set_results_);
}

const CascadeLayer* RuleSet::GetLayerForTest(const RuleData& rule) const {
  if (!layer_intervals_.size() ||
      layer_intervals_[0].start_position > rule.GetPosition())
    return implicit_outer_layer_;
  for (unsigned i = 1; i < layer_intervals_.size(); ++i) {
    if (layer_intervals_[i].start_position > rule.GetPosition())
      return layer_intervals_[i - 1].value;
  }
  return layer_intervals_.back().value;
}

void RuleData::Trace(Visitor* visitor) const {
  visitor->Trace(rule_);
}

template <class T>
void RuleSet::Interval<T>::Trace(Visitor* visitor) const {
  visitor->Trace(value);
}

void RuleSet::Trace(Visitor* visitor) const {
  visitor->Trace(id_rules_);
  visitor->Trace(class_rules_);
  visitor->Trace(attr_rules_);
  visitor->Trace(tag_rules_);
  visitor->Trace(ua_shadow_pseudo_element_rules_);
  visitor->Trace(link_pseudo_class_rules_);
  visitor->Trace(cue_pseudo_rules_);
  visitor->Trace(focus_pseudo_class_rules_);
  visitor->Trace(selector_fragment_anchor_rules_);
  visitor->Trace(focus_visible_pseudo_class_rules_);
  visitor->Trace(spatial_navigation_interest_class_rules_);
  visitor->Trace(universal_rules_);
  visitor->Trace(shadow_host_rules_);
  visitor->Trace(part_pseudo_rules_);
  visitor->Trace(slotted_pseudo_element_rules_);
  visitor->Trace(visited_dependent_rules_);
  visitor->Trace(page_rules_);
  visitor->Trace(font_face_rules_);
  visitor->Trace(font_palette_values_rules_);
  visitor->Trace(font_feature_values_rules_);
  visitor->Trace(keyframes_rules_);
  visitor->Trace(property_rules_);
  visitor->Trace(counter_style_rules_);
  visitor->Trace(position_fallback_rules_);
  visitor->Trace(media_query_set_results_);
  visitor->Trace(implicit_outer_layer_);
  visitor->Trace(layer_intervals_);
  visitor->Trace(container_query_intervals_);
  visitor->Trace(scope_intervals_);
#ifndef NDEBUG
  visitor->Trace(all_rules_);
#endif
}

#ifndef NDEBUG
void RuleSet::Show() const {
  for (const RuleData& rule : all_rules_)
    rule.Selector().Show();
}
#endif

}  // namespace blink
