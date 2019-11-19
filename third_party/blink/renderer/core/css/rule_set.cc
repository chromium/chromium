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
    if (component->GetPseudoType() == CSSSelector::kPseudoCue ||
        (component->Match() == CSSSelector::kPseudoElement &&
         component->Value() == TextTrackCue::CueShadowPseudoId()))
      return ValidPropertyFilter::kCue;
    if (component->GetPseudoType() == CSSSelector::kPseudoFirstLetter)
      return ValidPropertyFilter::kFirstLetter;
    if (component->GetPseudoType() == CSSSelector::kPseudoMarker)
      return ValidPropertyFilter::kMarker;
  }
  return ValidPropertyFilter::kNoFilter;
}

RuleData* RuleData::MaybeCreate(StyleRule* rule,
                                unsigned selector_index,
                                unsigned position,
                                AddRuleFlags add_rule_flags) {
  // The selector index field in RuleData is only 13 bits so we can't support
  // selectors at index 8192 or beyond.
  // See https://crbug.com/804179
  if (selector_index >= (1 << RuleData::kSelectorIndexBits))
    return nullptr;
  if (position >= (1 << RuleData::kPositionBits))
    return nullptr;
  return MakeGarbageCollected<RuleData>(rule, selector_index, position,
                                        add_rule_flags);
}

RuleData::RuleData(StyleRule* rule,
                   unsigned selector_index,
                   unsigned position,
                   AddRuleFlags add_rule_flags)
    : rule_(rule),
      selector_index_(selector_index),
      position_(position),
      specificity_(Selector().Specificity()),
      link_match_type_(Selector().ComputeLinkMatchType(CSSSelector::kMatchAll)),
      has_document_security_origin_(add_rule_flags &
                                    kRuleHasDocumentSecurityOrigin),
      valid_property_filter_(
          static_cast<std::underlying_type_t<ValidPropertyFilter>>(
              DetermineValidPropertyFilter(add_rule_flags, Selector()))),
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
                                  AtomicString& custom_pseudo_element_name,
                                  AtomicString& tag_name,
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
        case CSSSelector::kPseudoPlaceholder:
        case CSSSelector::kPseudoPart:
        case CSSSelector::kPseudoHost:
        case CSSSelector::kPseudoHostContext:
        case CSSSelector::kPseudoSpatialNavigationInterest:
          pseudo_type = selector->GetPseudoType();
          break;
        case CSSSelector::kPseudoWebKitCustomElement:
        case CSSSelector::kPseudoBlinkInternalElement:
          custom_pseudo_element_name = selector->Value();
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }
}

bool RuleSet::FindBestRuleSetAndAdd(const CSSSelector& component,
                                    RuleData* rule_data) {
  AtomicString id;
  AtomicString class_name;
  AtomicString custom_pseudo_element_name;
  AtomicString tag_name;
  CSSSelector::PseudoType pseudo_type = CSSSelector::kPseudoUnknown;

#ifndef NDEBUG
  all_rules_.push_back(rule_data);
#endif

  const CSSSelector* it = &component;
  for (; it && it->Relation() == CSSSelector::kSubSelector;
       it = it->TagHistory()) {
    ExtractSelectorValues(it, id, class_name, custom_pseudo_element_name,
                          tag_name, pseudo_type);
  }
  if (it) {
    ExtractSelectorValues(it, id, class_name, custom_pseudo_element_name,
                          tag_name, pseudo_type);
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

  if (!custom_pseudo_element_name.IsEmpty()) {
    // Custom pseudos come before ids and classes in the order of tagHistory,
    // and have a relation of ShadowPseudo between them. Therefore we should
    // never be a situation where ExtractSelectorValues finds id and
    // className in addition to custom pseudo.
    DCHECK(id.IsEmpty());
    DCHECK(class_name.IsEmpty());
    AddToRuleSet(custom_pseudo_element_name,
                 EnsurePendingRules()->shadow_pseudo_element_rules, rule_data);
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
    case CSSSelector::kPseudoPlaceholder:
      if (it->FollowsPart()) {
        part_pseudo_rules_.push_back(rule_data);
      } else {
        AddToRuleSet(AtomicString("-webkit-input-placeholder"),
                     EnsurePendingRules()->shadow_pseudo_element_rules,
                     rule_data);
      }
      return true;
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoHostContext:
      shadow_host_rules_.push_back(rule_data);
      return true;
    case CSSSelector::kPseudoPart:
      part_pseudo_rules_.push_back(rule_data);
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
                      AddRuleFlags add_rule_flags) {
  RuleData* rule_data =
      RuleData::MaybeCreate(rule, selector_index, rule_count_, add_rule_flags);
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

void RuleSet::AddChildRules(const HeapVector<Member<StyleRuleBase>>& rules,
                            const MediaQueryEvaluator& medium,
                            AddRuleFlags add_rule_flags) {
  for (unsigned i = 0; i < rules.size(); ++i) {
    StyleRuleBase* rule = rules[i].Get();

    if (auto* style_rule = DynamicTo<StyleRule>(rule)) {
      const CSSSelectorList& selector_list = style_rule->SelectorList();
      for (const CSSSelector* selector = selector_list.First(); selector;
           selector = selector_list.Next(*selector)) {
        wtf_size_t selector_index = selector_list.SelectorIndex(*selector);
        if (selector->HasDeepCombinatorOrShadowPseudo()) {
          deep_combinator_or_shadow_pseudo_rules_.push_back(
              MinimalRuleData(style_rule, selector_index, add_rule_flags));
        } else if (selector->HasContentPseudo()) {
          content_pseudo_element_rules_.push_back(
              MinimalRuleData(style_rule, selector_index, add_rule_flags));
        } else if (selector->HasSlottedPseudo()) {
          slotted_pseudo_element_rules_.push_back(
              MinimalRuleData(style_rule, selector_index, add_rule_flags));
        } else {
          AddRule(style_rule, selector_index, add_rule_flags);
        }
      }
    } else if (auto* page_rule = DynamicTo<StyleRulePage>(rule)) {
      AddPageRule(page_rule);
    } else if (auto* media_rule = DynamicTo<StyleRuleMedia>(rule)) {
      if (!media_rule->MediaQueries() ||
          medium.Eval(*media_rule->MediaQueries(),
                      &features_.ViewportDependentMediaQueryResults(),
                      &features_.DeviceDependentMediaQueryResults()))
        AddChildRules(media_rule->ChildRules(), medium, add_rule_flags);
    } else if (auto* font_face_rule = DynamicTo<StyleRuleFontFace>(rule)) {
      AddFontFaceRule(font_face_rule);
    } else if (auto* keyframes_rule = DynamicTo<StyleRuleKeyframes>(rule)) {
      AddKeyframesRule(keyframes_rule);
    } else if (auto* property_rule = DynamicTo<StyleRuleProperty>(rule)) {
      AddPropertyRule(property_rule);
    } else if (auto* supports_rule = DynamicTo<StyleRuleSupports>(rule)) {
      if (supports_rule->ConditionIsSupported())
        AddChildRules(supports_rule->ChildRules(), medium, add_rule_flags);
    }
  }
}

void RuleSet::AddRulesFromSheet(StyleSheetContents* sheet,
                                const MediaQueryEvaluator& medium,
                                AddRuleFlags add_rule_flags) {
  TRACE_EVENT0("blink", "RuleSet::addRulesFromSheet");

  DCHECK(sheet);

  const HeapVector<Member<StyleRuleImport>>& import_rules =
      sheet->ImportRules();
  for (unsigned i = 0; i < import_rules.size(); ++i) {
    StyleRuleImport* import_rule = import_rules[i].Get();
    if (import_rule->GetStyleSheet() &&
        (!import_rule->MediaQueries() ||
         medium.Eval(*import_rule->MediaQueries(),
                     &features_.ViewportDependentMediaQueryResults(),
                     &features_.DeviceDependentMediaQueryResults())))
      AddRulesFromSheet(import_rule->GetStyleSheet(), medium, add_rule_flags);
  }

  AddChildRules(sheet->ChildRules(), medium, add_rule_flags);
}

void RuleSet::AddStyleRule(StyleRule* rule, AddRuleFlags add_rule_flags) {
  for (wtf_size_t selector_index =
           rule->SelectorList().SelectorIndex(*rule->SelectorList().First());
       selector_index != kNotFound;
       selector_index =
           rule->SelectorList().IndexOfNextSelectorAfter(selector_index))
    AddRule(rule, selector_index, add_rule_flags);
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
    while (!pending_rules->IsEmpty()) {
      rules->push_back(pending_rules->Peek());
      pending_rules->Pop();
    }
  }
}

void RuleSet::CompactRules() {
  DCHECK(pending_rules_);
  PendingRuleMaps* pending_rules = pending_rules_.Release();
  CompactPendingRules(pending_rules->id_rules, id_rules_);
  CompactPendingRules(pending_rules->class_rules, class_rules_);
  CompactPendingRules(pending_rules->tag_rules, tag_rules_);
  CompactPendingRules(pending_rules->shadow_pseudo_element_rules,
                      shadow_pseudo_element_rules_);
  link_pseudo_class_rules_.ShrinkToFit();
  cue_pseudo_rules_.ShrinkToFit();
  focus_pseudo_class_rules_.ShrinkToFit();
  spatial_navigation_interest_class_rules_.ShrinkToFit();
  universal_rules_.ShrinkToFit();
  shadow_host_rules_.ShrinkToFit();
  page_rules_.ShrinkToFit();
  font_face_rules_.ShrinkToFit();
  keyframes_rules_.ShrinkToFit();
  property_rules_.ShrinkToFit();
  deep_combinator_or_shadow_pseudo_rules_.ShrinkToFit();
  part_pseudo_rules_.ShrinkToFit();
  content_pseudo_element_rules_.ShrinkToFit();
  slotted_pseudo_element_rules_.ShrinkToFit();
}

void MinimalRuleData::Trace(blink::Visitor* visitor) {
  visitor->Trace(rule_);
}

void RuleData::Trace(blink::Visitor* visitor) {
  visitor->Trace(rule_);
}

void RuleSet::PendingRuleMaps::Trace(blink::Visitor* visitor) {
  visitor->Trace(id_rules);
  visitor->Trace(class_rules);
  visitor->Trace(tag_rules);
  visitor->Trace(shadow_pseudo_element_rules);
}

void RuleSet::Trace(blink::Visitor* visitor) {
  visitor->Trace(id_rules_);
  visitor->Trace(class_rules_);
  visitor->Trace(tag_rules_);
  visitor->Trace(shadow_pseudo_element_rules_);
  visitor->Trace(link_pseudo_class_rules_);
  visitor->Trace(cue_pseudo_rules_);
  visitor->Trace(focus_pseudo_class_rules_);
  visitor->Trace(spatial_navigation_interest_class_rules_);
  visitor->Trace(universal_rules_);
  visitor->Trace(shadow_host_rules_);
  visitor->Trace(page_rules_);
  visitor->Trace(font_face_rules_);
  visitor->Trace(keyframes_rules_);
  visitor->Trace(property_rules_);
  visitor->Trace(deep_combinator_or_shadow_pseudo_rules_);
  visitor->Trace(part_pseudo_rules_);
  visitor->Trace(content_pseudo_element_rules_);
  visitor->Trace(slotted_pseudo_element_rules_);
  visitor->Trace(pending_rules_);
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
