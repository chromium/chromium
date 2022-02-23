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

#include <type_traits>

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

namespace blink {

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

RuleData* RuleData::MaybeCreate(StyleRule* rule,
                                unsigned selector_index,
                                unsigned position,
                                AddRuleFlags add_rule_flags,
                                const ContainerQuery* container_query) {
  // The selector index field in RuleData is only 13 bits so we can't support
  // selectors at index 8192 or beyond.
  // See https://crbug.com/804179
  if (selector_index >= (1 << RuleData::kSelectorIndexBits))
    return nullptr;
  if (position >= (1 << RuleData::kPositionBits))
    return nullptr;
  if (container_query) {
    return MakeGarbageCollected<ExtendedRuleData>(
        base::PassKey<RuleData>(), rule, selector_index, position,
        add_rule_flags, container_query);
  }
  return MakeGarbageCollected<RuleData>(rule, selector_index, position,
                                        add_rule_flags);
}

RuleData::RuleData(StyleRule* rule,
                   unsigned selector_index,
                   unsigned position,
                   AddRuleFlags add_rule_flags)
    : RuleData(Type::kNormal, rule, selector_index, position, add_rule_flags) {}

RuleData::RuleData(Type type,
                   StyleRule* rule,
                   unsigned selector_index,
                   unsigned position,
                   AddRuleFlags add_rule_flags)
    : rule_(rule),
      selector_index_(selector_index),
      position_(position),
      specificity_(Selector().Specificity()),
      link_match_type_(DetermineLinkMatchType(add_rule_flags, Selector())),
      has_document_security_origin_(add_rule_flags &
                                    kRuleHasDocumentSecurityOrigin),
      valid_property_filter_(
          static_cast<std::underlying_type_t<ValidPropertyFilter>>(
              DetermineValidPropertyFilter(add_rule_flags, Selector()))),
      type_(static_cast<unsigned>(type)),
      descendant_selector_identifier_hashes_() {
  SelectorFilter::CollectIdentifierHashes(
      Selector(), descendant_selector_identifier_hashes_,
      kMaximumIdentifierCount);
}

void RuleSet::AddToRuleSet(const AtomicString& key,
                           PendingRuleMap& map,
                           const RuleData* rule_data) {
  Member<HeapLinkedStack<Member<const RuleData>>>& rules =
      map.insert(key, nullptr).stored_value->value;
  if (!rules)
    rules = MakeGarbageCollected<HeapLinkedStack<Member<const RuleData>>>();
  rules->Push(rule_data);
}

static void ExtractSelectorValues(const CSSSelector* selector,
                                  AtomicString& id,
                                  AtomicString& class_name,
                                  AtomicString& attr_name,
                                  AtomicString& custom_pseudo_element_name,
                                  AtomicString& tag_name,
                                  AtomicString& part_name,
                                  CSSSelector::PseudoType& pseudo_type) {
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
        default:
          break;
      }
      break;
    case CSSSelector::kAttributeExact:
    case CSSSelector::kAttributeSet:
    case CSSSelector::kAttributeHyphen:
    case CSSSelector::kAttributeList:
    case CSSSelector::kAttributeContain:
    case CSSSelector::kAttributeBegin:
    case CSSSelector::kAttributeEnd:
      attr_name = selector->Attribute().LocalName();
      break;
    default:
      break;
  }
}

bool RuleSet::FindBestRuleSetAndAdd(const CSSSelector& component,
                                    RuleData* rule_data) {
  AtomicString id;
  AtomicString class_name;
  AtomicString attr_name;
  AtomicString custom_pseudo_element_name;
  AtomicString tag_name;
  AtomicString part_name;
  CSSSelector::PseudoType pseudo_type = CSSSelector::kPseudoUnknown;

#ifndef NDEBUG
  all_rules_.push_back(rule_data);
#endif

  const CSSSelector* it = &component;
  for (; it && it->Relation() == CSSSelector::kSubSelector;
       it = it->TagHistory()) {
    ExtractSelectorValues(it, id, class_name, attr_name,
                          custom_pseudo_element_name, tag_name, part_name,
                          pseudo_type);
  }
  if (it) {
    ExtractSelectorValues(it, id, class_name, attr_name,
                          custom_pseudo_element_name, tag_name, part_name,
                          pseudo_type);
  }

  // Prefer rule sets in order of most likely to apply infrequently.
  if (!id.IsEmpty()) {
    AddToRuleSet(id, EnsurePendingRules()->id_rules, rule_data);
    return true;
  }

  if (!class_name.IsEmpty()) {
    AddToRuleSet(class_name, EnsurePendingRules()->class_rules, rule_data);
    return true;
  }

  if (!attr_name.IsEmpty()) {
    AddToRuleSet(attr_name, EnsurePendingRules()->attr_rules, rule_data);
    if (attr_name == html_names::kStyleAttr) {
      has_bucket_for_style_attr_ = true;
    }
    return true;
  }

  if (!custom_pseudo_element_name.IsEmpty()) {
    // Custom pseudos come before ids and classes in the order of tagHistory,
    // and have a relation of ShadowPseudo between them. Therefore we should
    // never be a situation where ExtractSelectorValues finds id and
    // className in addition to custom pseudo.
    DCHECK(id.IsEmpty());
    DCHECK(class_name.IsEmpty());
    AddToRuleSet(custom_pseudo_element_name,
                 EnsurePendingRules()->ua_shadow_pseudo_element_rules,
                 rule_data);
    return true;
  }

  if (!part_name.IsEmpty()) {
    part_pseudo_rules_.push_back(rule_data);
    return true;
  }

  switch (pseudo_type) {
    case CSSSelector::kPseudoCue:
      cue_pseudo_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoLink:
    case CSSSelector::kPseudoVisited:
    case CSSSelector::kPseudoAnyLink:
    case CSSSelector::kPseudoWebkitAnyLink:
      link_pseudo_class_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoSpatialNavigationInterest:
      spatial_navigation_interest_class_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoFocus:
      focus_pseudo_class_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoSelectorFragmentAnchor:
      selector_fragment_anchor_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoFocusVisible:
      focus_visible_pseudo_class_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoPlaceholder:
    case CSSSelector::kPseudoFileSelectorButton:
      if (it->FollowsPart()) {
        part_pseudo_rules_.push_back(rule_data);
      } else if (it->FollowsSlotted()) {
        slotted_pseudo_element_rules_.push_back(rule_data);
      } else {
        const auto& name = pseudo_type == CSSSelector::kPseudoFileSelectorButton
                               ? shadow_element_names::kPseudoFileUploadButton
                               : shadow_element_names::kPseudoInputPlaceholder;
        AddToRuleSet(name, EnsurePendingRules()->ua_shadow_pseudo_element_rules,
                     rule_data);
      }
      return true;
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoHostContext:
      shadow_host_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoSlotted:
      slotted_pseudo_element_rules_.push_back(rule_data);
      return true;
    default:
      break;
  }

  if (!tag_name.IsEmpty()) {
    AddToRuleSet(tag_name, EnsurePendingRules()->tag_rules, rule_data);
    return true;
  }

  return false;
}

void RuleSet::AddRule(StyleRule* rule,
                      unsigned selector_index,
                      AddRuleFlags add_rule_flags,
                      const ContainerQuery* container_query,
                      const CascadeLayer* cascade_layer) {
  RuleData* rule_data = RuleData::MaybeCreate(rule, selector_index, rule_count_,
                                              add_rule_flags, container_query);
  if (!rule_data) {
    // This can happen if selector_index or position is out of range.
    return;
  }
  ++rule_count_;
  if (features_.CollectFeaturesFromRuleData(rule_data) ==
      RuleFeatureSet::kSelectorNeverMatches)
    return;

  if (!FindBestRuleSetAndAdd(rule_data->Selector(), rule_data)) {
    // If we didn't find a specialized map to stick it in, file under universal
    // rules.
    universal_rules_.push_back(rule_data);
  }

  // If the rule has CSSSelector::kMatchLink, it means that there is a :visited
  // or :link pseudo-class somewhere in the selector. In those cases, we
  // effectively split the rule into two: one which covers the situation
  // where we are in an unvisited link (kMatchLink), and another which covers
  // the visited link case (kMatchVisited).
  if (rule_data->LinkMatchType() == CSSSelector::kMatchLink) {
    RuleData* visited_dependent = RuleData::MaybeCreate(
        rule, rule_data->SelectorIndex(), rule_data->GetPosition(),
        add_rule_flags | kRuleIsVisitedDependent, container_query);
    DCHECK(visited_dependent);
    visited_dependent_rules_.push_back(visited_dependent);
  }

  if (RuntimeEnabledFeatures::CSSCascadeLayersEnabled())
    AddRuleToLayerIntervals(cascade_layer, rule_data->GetPosition());
}

void RuleSet::AddRuleToLayerIntervals(const CascadeLayer* cascade_layer,
                                      unsigned position) {
  // Add a new interval only if the current layer is different from the last
  // interval's layer. Note that the implicit outer layer may also be
  // represented by a nullptr.
  const CascadeLayer* last_interval_layer =
      layer_intervals_.size() ? layer_intervals_.back().layer.Get()
                              : implicit_outer_layer_.Get();
  if (!cascade_layer)
    cascade_layer = implicit_outer_layer_;
  if (cascade_layer == last_interval_layer)
    return;

  if (!cascade_layer)
    cascade_layer = EnsureImplicitOuterLayer();
  layer_intervals_.push_back(LayerInterval(cascade_layer, position));
}

void RuleSet::AddPageRule(StyleRulePage* rule) {
  EnsurePendingRules();  // So that page_rules_.ShrinkToFit() gets called.
  page_rules_.push_back(rule);
}

void RuleSet::AddFontFaceRule(StyleRuleFontFace* rule) {
  EnsurePendingRules();  // So that font_face_rules_.ShrinkToFit() gets called.
  font_face_rules_.push_back(rule);
}

void RuleSet::AddKeyframesRule(StyleRuleKeyframes* rule) {
  EnsurePendingRules();  // So that keyframes_rules_.ShrinkToFit() gets called.
  keyframes_rules_.push_back(rule);
}

void RuleSet::AddPropertyRule(StyleRuleProperty* rule) {
  EnsurePendingRules();  // So that property_rules_.ShrinkToFit() gets called.
  property_rules_.push_back(rule);
}

void RuleSet::AddCounterStyleRule(StyleRuleCounterStyle* rule) {
  EnsurePendingRules();  // So that counter_style_rules_.ShrinkToFit() gets
                         // called.
  counter_style_rules_.push_back(rule);
}

void RuleSet::AddFontPaletteValuesRule(StyleRuleFontPaletteValues* rule) {
  EnsurePendingRules();
  font_palette_values_rules_.push_back(rule);
}

void RuleSet::AddScrollTimelineRule(StyleRuleScrollTimeline* rule) {
  EnsurePendingRules();  // So that property_rules_.ShrinkToFit() gets called.
  scroll_timeline_rules_.push_back(rule);
}

void RuleSet::AddChildRules(const HeapVector<Member<StyleRuleBase>>& rules,
                            const MediaQueryEvaluator& medium,
                            AddRuleFlags add_rule_flags,
                            const ContainerQuery* container_query,
                            CascadeLayer* cascade_layer) {
  for (unsigned i = 0; i < rules.size(); ++i) {
    StyleRuleBase* rule = rules[i].Get();

    if (auto* style_rule = DynamicTo<StyleRule>(rule)) {
      const CSSSelectorList& selector_list = style_rule->SelectorList();
      for (const CSSSelector* selector = selector_list.First(); selector;
           selector = selector_list.Next(*selector)) {
        wtf_size_t selector_index = selector_list.SelectorIndex(*selector);
        AddRule(style_rule, selector_index, add_rule_flags, container_query,
                cascade_layer);
      }
    } else if (auto* page_rule = DynamicTo<StyleRulePage>(rule)) {
      page_rule->SetCascadeLayer(cascade_layer);
      AddPageRule(page_rule);
    } else if (auto* media_rule = DynamicTo<StyleRuleMedia>(rule)) {
      if (MatchMediaForAddRules(medium, media_rule->MediaQueries())) {
        AddChildRules(media_rule->ChildRules(), medium, add_rule_flags,
                      container_query, cascade_layer);
      }
    } else if (auto* font_face_rule = DynamicTo<StyleRuleFontFace>(rule)) {
      font_face_rule->SetCascadeLayer(cascade_layer);
      AddFontFaceRule(font_face_rule);
    } else if (auto* font_palette_values_rule =
                   DynamicTo<StyleRuleFontPaletteValues>(rule)) {
      // TODO(https://crbug.com/1170794): Handle cascade layers for
      // @font-palette-values.
      AddFontPaletteValuesRule(font_palette_values_rule);
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
    } else if (auto* scroll_timeline_rule =
                   DynamicTo<StyleRuleScrollTimeline>(rule)) {
      scroll_timeline_rule->SetCascadeLayer(cascade_layer);
      AddScrollTimelineRule(scroll_timeline_rule);
    } else if (auto* supports_rule = DynamicTo<StyleRuleSupports>(rule)) {
      if (supports_rule->ConditionIsSupported()) {
        AddChildRules(supports_rule->ChildRules(), medium, add_rule_flags,
                      container_query, cascade_layer);
      }
    } else if (auto* container_rule = DynamicTo<StyleRuleContainer>(rule)) {
      const ContainerQuery* inner_container_query =
          &container_rule->GetContainerQuery();
      if (container_query) {
        inner_container_query =
            inner_container_query->CopyWithParent(container_query);
      }
      AddChildRules(container_rule->ChildRules(), medium, add_rule_flags,
                    inner_container_query, cascade_layer);
    } else if (auto* layer_block_rule = DynamicTo<StyleRuleLayerBlock>(rule)) {
      CascadeLayer* sub_layer =
          GetOrAddSubLayer(cascade_layer, layer_block_rule->GetName());
      AddChildRules(layer_block_rule->ChildRules(), medium, add_rule_flags,
                    container_query, sub_layer);
    } else if (auto* layer_statement_rule =
                   DynamicTo<StyleRuleLayerStatement>(rule)) {
      for (const auto& layer_name : layer_statement_rule->GetNames())
        GetOrAddSubLayer(cascade_layer, layer_name);
    }
  }
}

bool RuleSet::MatchMediaForAddRules(const MediaQueryEvaluator& evaluator,
                                    const MediaQuerySet* media_queries) {
  if (!media_queries)
    return true;
  bool match_media = evaluator.Eval(
      *media_queries, MediaQueryEvaluator::Results{
                          &features_.ViewportDependentMediaQueryResults(),
                          &features_.DeviceDependentMediaQueryResults(),
                          &features_.MediaQueryUnitFlags()});
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
                nullptr /* container_query */, cascade_layer);
}

void RuleSet::AddStyleRule(StyleRule* rule, AddRuleFlags add_rule_flags) {
  for (wtf_size_t selector_index =
           rule->SelectorList().SelectorIndex(*rule->SelectorList().First());
       selector_index != kNotFound;
       selector_index =
           rule->SelectorList().IndexOfNextSelectorAfter(selector_index)) {
    AddRule(rule, selector_index, add_rule_flags, nullptr /* container_query */,
            nullptr /* cascade_layer */);
  }
}

CascadeLayer* RuleSet::GetOrAddSubLayer(CascadeLayer* cascade_layer,
                                        const StyleRuleBase::LayerName& name) {
  if (!cascade_layer)
    cascade_layer = EnsureImplicitOuterLayer();
  return cascade_layer->GetOrAddSubLayer(name);
}

void RuleSet::CompactPendingRules(PendingRuleMap& pending_map,
                                  CompactRuleMap& compact_map) {
  for (auto& item : pending_map) {
    HeapLinkedStack<Member<const RuleData>>* pending_rules =
        item.value.Release();
    Member<HeapVector<Member<const RuleData>>>& rules =
        compact_map.insert(item.key, nullptr).stored_value->value;
    if (!rules) {
      rules = MakeGarbageCollected<HeapVector<Member<const RuleData>>>();
      rules->ReserveInitialCapacity(pending_rules->size());
    } else {
      rules->ReserveCapacity(pending_rules->size());
    }
    // Since pending_rules is a stack, we need to insert in the reversed
    // ordering so that the resulting vector is sorted by rule position
    wtf_size_t num_pending_rules = pending_rules->size();
    rules->Grow(rules->size() + num_pending_rules);
    for (auto iter = rules->rbegin(); !pending_rules->IsEmpty(); ++iter) {
      DCHECK(iter != rules->rend());
      *iter = pending_rules->Peek();
      pending_rules->Pop();
    }
  }
}

void RuleSet::CompactRules() {
  DCHECK(pending_rules_);
  PendingRuleMaps* pending_rules = pending_rules_.Release();
  CompactPendingRules(pending_rules->id_rules, id_rules_);
  CompactPendingRules(pending_rules->class_rules, class_rules_);
  CompactPendingRules(pending_rules->attr_rules, attr_rules_);
  CompactPendingRules(pending_rules->tag_rules, tag_rules_);
  CompactPendingRules(pending_rules->ua_shadow_pseudo_element_rules,
                      ua_shadow_pseudo_element_rules_);
  link_pseudo_class_rules_.ShrinkToFit();
  cue_pseudo_rules_.ShrinkToFit();
  focus_pseudo_class_rules_.ShrinkToFit();
  selector_fragment_anchor_rules_.ShrinkToFit();
  focus_visible_pseudo_class_rules_.ShrinkToFit();
  spatial_navigation_interest_class_rules_.ShrinkToFit();
  universal_rules_.ShrinkToFit();
  shadow_host_rules_.ShrinkToFit();
  part_pseudo_rules_.ShrinkToFit();
  slotted_pseudo_element_rules_.ShrinkToFit();
  visited_dependent_rules_.ShrinkToFit();
  page_rules_.ShrinkToFit();
  font_face_rules_.ShrinkToFit();
  font_palette_values_rules_.ShrinkToFit();
  keyframes_rules_.ShrinkToFit();
  property_rules_.ShrinkToFit();
  counter_style_rules_.ShrinkToFit();
  scroll_timeline_rules_.ShrinkToFit();
  layer_intervals_.ShrinkToFit();

#if EXPENSIVE_DCHECKS_ARE_ON()
  AssertRuleListsSorted();
#endif
}

#if EXPENSIVE_DCHECKS_ARE_ON()

namespace {

template <class RuleList>
bool IsRuleListSorted(const RuleList& rules) {
  unsigned last_position = 0;
  bool first_rule = true;
  for (const auto& rule : rules) {
    if (!first_rule && rule->GetPosition() <= last_position)
      return false;
    first_rule = false;
    last_position = rule->GetPosition();
  }
  return true;
}

}  // namespace

void RuleSet::AssertRuleListsSorted() const {
  for (const auto& item : id_rules_)
    DCHECK(IsRuleListSorted(*item.value));
  for (const auto& item : class_rules_)
    DCHECK(IsRuleListSorted(*item.value));
  for (const auto& item : tag_rules_)
    DCHECK(IsRuleListSorted(*item.value));
  for (const auto& item : ua_shadow_pseudo_element_rules_)
    DCHECK(IsRuleListSorted(*item.value));
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

const ContainerQuery* RuleData::GetContainerQuery() const {
  if (auto* extended = DynamicTo<ExtendedRuleData>(this))
    return extended->container_query_;
  return nullptr;
}

const CascadeLayer* RuleSet::GetLayerForTest(const RuleData& rule) const {
  DCHECK(RuntimeEnabledFeatures::CSSCascadeLayersEnabled());
  if (!layer_intervals_.size() ||
      layer_intervals_[0].start_position > rule.GetPosition())
    return implicit_outer_layer_;
  for (unsigned i = 1; i < layer_intervals_.size(); ++i) {
    if (layer_intervals_[i].start_position > rule.GetPosition())
      return layer_intervals_[i - 1].layer;
  }
  return layer_intervals_.back().layer;
}

void RuleData::Trace(Visitor* visitor) const {
  switch (static_cast<Type>(type_)) {
    case Type::kNormal:
      TraceAfterDispatch(visitor);
      break;
    case Type::kExtended:
      To<ExtendedRuleData>(*this).TraceAfterDispatch(visitor);
      break;
  }
}

void RuleData::TraceAfterDispatch(blink::Visitor* visitor) const {
  visitor->Trace(rule_);
}

ExtendedRuleData::ExtendedRuleData(base::PassKey<RuleData>,
                                   StyleRule* rule,
                                   unsigned selector_index,
                                   unsigned position,
                                   AddRuleFlags flags,
                                   const ContainerQuery* container_query)
    : RuleData(Type::kExtended, rule, selector_index, position, flags),
      container_query_(container_query) {}

void ExtendedRuleData::TraceAfterDispatch(Visitor* visitor) const {
  RuleData::TraceAfterDispatch(visitor);
  visitor->Trace(container_query_);
}

void RuleSet::PendingRuleMaps::Trace(Visitor* visitor) const {
  visitor->Trace(id_rules);
  visitor->Trace(class_rules);
  visitor->Trace(attr_rules);
  visitor->Trace(tag_rules);
  visitor->Trace(ua_shadow_pseudo_element_rules);
}

void RuleSet::LayerInterval::Trace(Visitor* visitor) const {
  visitor->Trace(layer);
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
  visitor->Trace(keyframes_rules_);
  visitor->Trace(property_rules_);
  visitor->Trace(counter_style_rules_);
  visitor->Trace(scroll_timeline_rules_);
  visitor->Trace(pending_rules_);
  visitor->Trace(implicit_outer_layer_);
  visitor->Trace(layer_intervals_);
#ifndef NDEBUG
  visitor->Trace(all_rules_);
#endif
}

#ifndef NDEBUG
void RuleSet::Show() const {
  for (const auto& rule : all_rules_)
    rule->Selector().Show();
}
#endif

}  // namespace blink
