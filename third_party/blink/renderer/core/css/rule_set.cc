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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/core/css/rule_set.h"

#include <memory>
#include <type_traits>
#include <vector>

#include "base/containers/contains.h"
#include "base/substring_set_matcher/substring_set_matcher.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/css_selector_list.h"
#include "third_party/blink/renderer/core/css/robin_hood_map-inl.h"
#include "third_party/blink/renderer/core/css/seeker.h"
#include "third_party/blink/renderer/core/css/selector_checker-inl.h"
#include "third_party/blink/renderer/core/css/selector_checker.h"
#include "third_party/blink/renderer/core/css/selector_filter.h"
#include "third_party/blink/renderer/core/css/style_rule_import.h"
#include "third_party/blink/renderer/core/css/style_rule_nested_declarations.h"
#include "third_party/blink/renderer/core/css/style_sheet_contents.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_utils.h"
#include "third_party/blink/renderer/core/html/track/text_track_cue.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/invalidation_set_to_selector_map.h"
#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

using base::MatcherStringPattern;
using base::SubstringSetMatcher;

namespace blink {

template <class T>
static void AddRuleToIntervals(const T* value,
                               unsigned position,
                               HeapVector<RuleSet::Interval<T>>& intervals);

static void UnmarkAsCoveredByBucketing(CSSSelector& selector);

static inline ValidPropertyFilter DetermineValidPropertyFilter(
    const AddRuleFlags add_rule_flags,
    const CSSSelector& selector) {
  for (const CSSSelector* component = &selector; component;
       component = component->NextSimpleSelector()) {
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
      case CSSSelector::kPseudoHighlight:
        if (UsesHighlightPseudoInheritance(
                component->GetPseudoId(component->GetPseudoType()))) {
          return ValidPropertyFilter::kHighlight;
        } else {
          return ValidPropertyFilter::kHighlightLegacy;
        }
      default:
        break;
    }
  }
  return ValidPropertyFilter::kNoFilter;
}

static bool SelectorListHasLinkOrVisited(const CSSSelector* selector_list) {
  for (const CSSSelector* complex = selector_list; complex;
       complex = CSSSelectorList::Next(*complex)) {
    if (complex->HasLinkOrVisited()) {
      return true;
    }
  }
  return false;
}

static bool StyleScopeHasLinkOrVisited(const StyleScope* style_scope) {
  return style_scope && (SelectorListHasLinkOrVisited(style_scope->From()) ||
                         SelectorListHasLinkOrVisited(style_scope->To()));
}

static unsigned DetermineLinkMatchType(const AddRuleFlags add_rule_flags,
                                       const CSSSelector& selector,
                                       const StyleScope* style_scope) {
  if (selector.HasLinkOrVisited() || StyleScopeHasLinkOrVisited(style_scope)) {
    return (add_rule_flags & kRuleIsVisitedDependent)
               ? CSSSelector::kMatchVisited
               : CSSSelector::kMatchLink;
  }
  return CSSSelector::kMatchAll;
}

RuleData::RuleData(StyleRule* rule,
                   unsigned selector_index,
                   unsigned position,
                   const StyleScope* style_scope,
                   AddRuleFlags add_rule_flags,
                   Vector<unsigned>& bloom_hash_backing)
    : rule_(rule),
      selector_index_(selector_index),
      position_(position),
      specificity_(Selector().Specificity()),
      link_match_type_(
          DetermineLinkMatchType(add_rule_flags, Selector(), style_scope)),
      valid_property_filter_(
          static_cast<std::underlying_type_t<ValidPropertyFilter>>(
              DetermineValidPropertyFilter(add_rule_flags, Selector()))),
      is_entirely_covered_by_bucketing_(
          false),  // Will be computed in ComputeEntirelyCoveredByBucketing().
      is_easy_(false),  // Ditto.
      is_starting_style_((add_rule_flags & kRuleIsStartingStyle) != 0),
      bloom_hash_size_(0),
      bloom_hash_pos_(0) {
  ComputeBloomFilterHashes(style_scope, bloom_hash_backing);
}

void RuleData::ComputeEntirelyCoveredByBucketing() {
  is_easy_ = EasySelectorChecker::IsEasy(&Selector());
  is_entirely_covered_by_bucketing_ = true;
  for (const CSSSelector* selector = &Selector(); selector;
       selector = selector->NextSimpleSelector()) {
    if (!selector->IsCoveredByBucketing()) {
      is_entirely_covered_by_bucketing_ = false;
      break;
    }
  }
}

void RuleData::ResetEntirelyCoveredByBucketing() {
  for (CSSSelector* selector = &MutableSelector(); selector;
       selector = selector->NextSimpleSelector()) {
    selector->SetCoveredByBucketing(false);
    if (selector->Relation() != CSSSelector::kSubSelector) {
      break;
    }
  }
  is_entirely_covered_by_bucketing_ = false;
}

void RuleData::ComputeBloomFilterHashes(const StyleScope* style_scope,
                                        Vector<unsigned>& bloom_hash_backing) {
  if (bloom_hash_backing.size() >= 16777216) {
    // This won't fit into bloom_hash_pos_, so don't collect any hashes.
    return;
  }
  bloom_hash_pos_ = bloom_hash_backing.size();
  SelectorFilter::CollectIdentifierHashes(Selector(), style_scope,
                                          bloom_hash_backing);

  // The clamp here is purely for safety; a real rule would never have
  // as many as 255 descendant selectors.
  bloom_hash_size_ =
      std::min<uint32_t>(bloom_hash_backing.size() - bloom_hash_pos_, 255);

  // If we've already got the exact same set of hashes in the vector,
  // we can simply reuse those, saving a bit of memory and cache space.
  // We only check the trivial case of a tail match; we could go with
  // something like a full suffix tree solution, but this is simple and
  // captures most of the benefits. (It is fairly common, especially with
  // nesting, to have the same sets of parents in consecutive rules.)
  if (bloom_hash_size_ > 0 && bloom_hash_pos_ >= bloom_hash_size_ &&
      std::equal(
          bloom_hash_backing.begin() + bloom_hash_pos_ - bloom_hash_size_,
          bloom_hash_backing.begin() + bloom_hash_pos_,
          bloom_hash_backing.begin() + bloom_hash_pos_)) {
    bloom_hash_backing.resize(bloom_hash_pos_);
    bloom_hash_pos_ -= bloom_hash_size_;
  }
}

void RuleData::MovedToDifferentRuleSet(const Vector<unsigned>& old_backing,
                                       Vector<unsigned>& new_backing,
                                       unsigned new_position) {
  unsigned new_pos = new_backing.size();
  new_backing.insert(new_backing.size(), old_backing.data() + bloom_hash_pos_,
                     bloom_hash_size_);
  bloom_hash_pos_ = new_pos;
  position_ = new_position;
}

void RuleSet::AddToRuleSet(const AtomicString& key,
                           RuleMap& map,
                           const RuleData& rule_data) {
  if (map.IsCompacted()) {
    // This normally should not happen, but may with UA stylesheets;
    // see class comment on RuleMap.
    map.Uncompact();
  }
  if (!map.Add(key, rule_data)) {
    // This should really only happen in case of an attack;
    // we stick it in the universal bucket so that correctness
    // is preserved, even though the performance will be suboptimal.
    RuleData rule_data_copy = rule_data;
    UnmarkAsCoveredByBucketing(rule_data_copy.MutableSelector());
    AddToRuleSet(universal_rules_, rule_data_copy);
    return;
  }
  // Don't call ComputeBloomFilterHashes() here; RuleMap needs that space for
  // group information, and will call ComputeBloomFilterHashes() itself on
  // compaction.
  need_compaction_ = true;
}

void RuleSet::AddToRuleSet(HeapVector<RuleData>& rules,
                           const RuleData& rule_data) {
  rules.push_back(rule_data);
  rules.back().ComputeEntirelyCoveredByBucketing();
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
                                  AtomicString& picker_name,
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
          CSSSelector::UniversalSelectorAtom()) {
        tag_name = selector->TagQName().LocalName();
      }
      break;
    case CSSSelector::kPseudoClass:
    case CSSSelector::kPseudoElement:
    case CSSSelector::kPagePseudoClass:
      // Must match the cases in RuleSet::FindBestRuleSetAndAdd.
      switch (selector->GetPseudoType()) {
        case CSSSelector::kPseudoFocus:
          if (pseudo_type == CSSSelector::kPseudoScrollMarker ||
              pseudo_type == CSSSelector::kPseudoScrollNextButton ||
              pseudo_type == CSSSelector::kPseudoScrollPrevButton) {
            break;
          }
          [[fallthrough]];
        case CSSSelector::kPseudoCue:
        case CSSSelector::kPseudoLink:
        case CSSSelector::kPseudoVisited:
        case CSSSelector::kPseudoWebkitAnyLink:
        case CSSSelector::kPseudoAnyLink:
        case CSSSelector::kPseudoFocusVisible:
        case CSSSelector::kPseudoPlaceholder:
        case CSSSelector::kPseudoDetailsContent:
        case CSSSelector::kPseudoFileSelectorButton:
        case CSSSelector::kPseudoHost:
        case CSSSelector::kPseudoHostContext:
        case CSSSelector::kPseudoSlotted:
        case CSSSelector::kPseudoSelectorFragmentAnchor:
        case CSSSelector::kPseudoRoot:
        case CSSSelector::kPseudoScrollMarker:
        case CSSSelector::kPseudoScrollNextButton:
        case CSSSelector::kPseudoScrollPrevButton:
          pseudo_type = selector->GetPseudoType();
          break;
        case CSSSelector::kPseudoWebKitCustomElement:
        case CSSSelector::kPseudoBlinkInternalElement:
          custom_pseudo_element_name = selector->Value();
          break;
        case CSSSelector::kPseudoPart:
          part_name = selector->Value();
          break;
        case CSSSelector::kPseudoPicker:
          picker_name = selector->Argument();
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
                                  part_name, picker_name, pseudo_type);
          }
        } break;
        default:
          break;
      }
      break;
    case CSSSelector::kAttributeSet:
      attr_name = selector->Attribute().LocalName();
      attr_value = g_empty_atom;
      break;
    case CSSSelector::kAttributeExact:
      is_exact_attr = true;
      [[fallthrough]];
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
    AtomicString& picker_name,
    CSSSelector::PseudoType& pseudo_type) {
  const CSSSelector* it = &component;
  for (; it && it->Relation() == CSSSelector::kSubSelector;
       it = it->NextSimpleSelector()) {
    ExtractSelectorValues(it, id, class_name, attr_name, attr_value,
                          is_exact_attr, custom_pseudo_element_name, tag_name,
                          part_name, picker_name, pseudo_type);
  }
  if (it) {
    ExtractSelectorValues(it, id, class_name, attr_name, attr_value,
                          is_exact_attr, custom_pseudo_element_name, tag_name,
                          part_name, picker_name, pseudo_type);
  }
  return it;
}

template <class Func>
static void MarkAsCoveredByBucketing(CSSSelector& selector,
                                     Func&& should_mark_func) {
  for (CSSSelector* s = &selector;;
       ++s) {  // Termination condition within loop.
    if (should_mark_func(*s)) {
      s->SetCoveredByBucketing(true);
    }

    // NOTE: We could also have tested single-element :is() and :where()
    // if the inside matches, but it's very rare, so we save the runtime
    // here instead. (& in nesting selectors could perhaps be somewhat
    // more common, but we currently don't bucket on & at all.)
    //
    // We could also have taken universal selectors no matter what
    // should_mark_func() says, but again, we consider that not worth it
    // (though if the selector is being put in the universal bucket,
    // there will be an explicit check).

    if (s->IsLastInComplexSelector() ||
        s->Relation() != CSSSelector::kSubSelector) {
      break;
    }
  }
}

static void UnmarkAsCoveredByBucketing(CSSSelector& selector) {
  for (CSSSelector* s = &selector;;
       ++s) {  // Termination condition within loop.
    s->SetCoveredByBucketing(false);
    if (s->IsLastInComplexSelector() ||
        s->Relation() != CSSSelector::kSubSelector) {
      break;
    }
  }
}

template <RuleSet::BucketCoverage bucket_coverage>
void RuleSet::FindBestRuleSetAndAdd(CSSSelector& component,
                                    const RuleData& rule_data) {
  AtomicString id;
  AtomicString class_name;
  AtomicString attr_name;
  AtomicString attr_value;  // Unused.
  AtomicString custom_pseudo_element_name;
  AtomicString tag_name;
  AtomicString part_name;
  AtomicString picker_name;
  CSSSelector::PseudoType pseudo_type = CSSSelector::kPseudoUnknown;

#if DCHECK_IS_ON()
  all_rules_.push_back(rule_data);
#endif  // DCHECK_IS_ON()

  bool is_exact_attr;
  const CSSSelector* it = ExtractBestSelectorValues(
      component, id, class_name, attr_name, attr_value, is_exact_attr,
      custom_pseudo_element_name, tag_name, part_name, picker_name,
      pseudo_type);

  // Prefer rule sets in order of most likely to apply infrequently.
  if (!id.empty()) {
    if (bucket_coverage == BucketCoverage::kCompute) {
      MarkAsCoveredByBucketing(component, [&id](const CSSSelector& selector) {
        return selector.Match() == CSSSelector::kId && selector.Value() == id;
      });
    }
    AddToRuleSet(id, id_rules_, rule_data);
    return;
  }

  if (!class_name.empty()) {
    if (bucket_coverage == BucketCoverage::kCompute) {
      MarkAsCoveredByBucketing(
          component, [&class_name](const CSSSelector& selector) {
            return selector.Match() == CSSSelector::kClass &&
                   selector.Value() == class_name;
          });
    }
    AddToRuleSet(class_name, class_rules_, rule_data);
    return;
  }

  if (!attr_name.empty()) {
    AddToRuleSet(attr_name, attr_rules_, rule_data);
    if (attr_name == html_names::kStyleAttr) {
      has_bucket_for_style_attr_ = true;
    }
    // NOTE: Cannot mark anything as covered by bucketing, since the bucketing
    // does not verify namespaces. (We could consider doing so if the namespace
    // is *, but we'd need to be careful about case sensitivity wrt. legacy
    // attributes.)
    return;
  }

  auto get_ua_shadow_pseudo = [&]() -> const AtomicString& {
    if (picker_name == "select") {
      return shadow_element_names::kPickerSelect;
    } else if (pseudo_type != CSSSelector::kPseudoUnknown) {
      return shadow_element_utils::StringForUAShadowPseudoId(
          CSSSelector::GetPseudoId(pseudo_type));
    }
    return g_null_atom;
  };

  AtomicString ua_shadow_pseudo = get_ua_shadow_pseudo();

  if (RuntimeEnabledFeatures::CSSCascadeCorrectScopeEnabled()) {
    // Any selector with or following ::part() or a UA shadow pseudo-element
    // must go in the bucket for the *innermost* such pseudo-element.

    // TODO(dbaron): Should this eventually check kShadowSlot as well?
    if (part_name.empty() && ua_shadow_pseudo == g_null_atom && it &&
        (it->Relation() == CSSSelector::RelationType::kUAShadow ||
         it->Relation() == CSSSelector::RelationType::kShadowPart)) {
      const CSSSelector* previous = it->NextSimpleSelector();
      if (previous->Match() == CSSSelector::kPseudoElement) {
        ExtractSelectorValues(previous, id, class_name, attr_name, attr_value,
                              is_exact_attr, custom_pseudo_element_name,
                              tag_name, part_name, picker_name, pseudo_type);
        ua_shadow_pseudo = get_ua_shadow_pseudo();
      }
    }
  }

  // Any selector with or following ::part() must go in the part bucket,
  // because we look in that bucket in higher scopes to find rules that need
  // to match inside the shadow tree.
  if (!part_name.empty() ||
      (it && it->FollowsPart() &&
       !RuntimeEnabledFeatures::CSSCascadeCorrectScopeEnabled())) {
    // NOTE: Cannot mark as covered by bucketing because the part buckets are
    // shared between the part itself and pseudo-elements inside of them.
    // (Though we do check at least some of the relevant conditions *before*
    // we check whether the selector is covered by bucketing, so it might be
    // doable if we want.)
    // TODO(https://crbug.com/40280846): When the CSSCascadeCorrectScope flag
    // is removed (and enabled), we can revisit this.
    AddToRuleSet(part_pseudo_rules_, rule_data);
    return;
  }

  if (!custom_pseudo_element_name.empty()) {
    // Custom pseudos come before ids and classes in the order of
    // NextSimpleSelector(), and have a relation of ShadowPseudo between them.
    // Therefore we should never be a situation where ExtractSelectorValues
    // finds id and className in addition to custom pseudo.
    DCHECK(id.empty());
    DCHECK(class_name.empty());
    AddToRuleSet(custom_pseudo_element_name, ua_shadow_pseudo_element_rules_,
                 rule_data);
    // TODO: Mark as covered by bucketing?
    return;
  }

  if (ua_shadow_pseudo != g_null_atom) {
    // TODO(dbaron): This needs further work to support multiple
    // pseudo-elements after ::slotted().  This likely requires reorganization
    // of how MatchSlottedRules interacts with MatchOuterScopeRules.
    if (it->FollowsSlotted()) {
      AddToRuleSet(slotted_pseudo_element_rules_, rule_data);
    } else {
      AddToRuleSet(ua_shadow_pseudo, ua_shadow_pseudo_element_rules_,
                   rule_data);
    }
    return;
  }

  switch (pseudo_type) {
    case CSSSelector::kPseudoCue:
      AddToRuleSet(cue_pseudo_rules_, rule_data);
      return;
    case CSSSelector::kPseudoLink:
    case CSSSelector::kPseudoVisited:
    case CSSSelector::kPseudoAnyLink:
    case CSSSelector::kPseudoWebkitAnyLink:
      if (bucket_coverage == BucketCoverage::kCompute) {
        MarkAsCoveredByBucketing(component, [](const CSSSelector& selector) {
          // We can only mark kPseudoAnyLink as checked by bucketing;
          // CollectMatchingRules() does not pre-check e.g. whether
          // the link is visible or not.
          return selector.Match() == CSSSelector::kPseudoClass &&
                 (selector.GetPseudoType() == CSSSelector::kPseudoAnyLink ||
                  selector.GetPseudoType() ==
                      CSSSelector::kPseudoWebkitAnyLink);
        });
      }
      AddToRuleSet(link_pseudo_class_rules_, rule_data);
      return;
    case CSSSelector::kPseudoFocus:
      if (bucket_coverage == BucketCoverage::kCompute) {
        MarkAsCoveredByBucketing(component, [](const CSSSelector& selector) {
          return selector.Match() == CSSSelector::kPseudoClass &&
                 selector.GetPseudoType() == CSSSelector::kPseudoFocus;
        });
      }
      AddToRuleSet(focus_pseudo_class_rules_, rule_data);
      return;
    case CSSSelector::kPseudoSelectorFragmentAnchor:
      AddToRuleSet(selector_fragment_anchor_rules_, rule_data);
      return;
    case CSSSelector::kPseudoFocusVisible:
      if (bucket_coverage == BucketCoverage::kCompute) {
        MarkAsCoveredByBucketing(component, [](const CSSSelector& selector) {
          return selector.Match() == CSSSelector::kPseudoClass &&
                 selector.GetPseudoType() == CSSSelector::kPseudoFocusVisible;
        });
      }
      AddToRuleSet(focus_visible_pseudo_class_rules_, rule_data);
      return;
    case CSSSelector::kPseudoHost:
    case CSSSelector::kPseudoHostContext:
      AddToRuleSet(shadow_host_rules_, rule_data);
      return;
    case CSSSelector::kPseudoSlotted:
      AddToRuleSet(slotted_pseudo_element_rules_, rule_data);
      return;
    case CSSSelector::kPseudoRoot:
      if (bucket_coverage == BucketCoverage::kCompute) {
        MarkAsCoveredByBucketing(component, [](const CSSSelector& selector) {
          return selector.Match() == CSSSelector::kPseudoClass &&
                 selector.GetPseudoType() == CSSSelector::kPseudoRoot;
        });
      }
      AddToRuleSet(root_element_rules_, rule_data);
      return;
    default:
      break;
  }

  if (!tag_name.empty()) {
    // Covered by bucketing only if the selector would match any namespace
    // (since the bucketing does not take the namespace into account).
    if (bucket_coverage == BucketCoverage::kCompute) {
      MarkAsCoveredByBucketing(
          component, [&tag_name](const CSSSelector& selector) {
            return selector.Match() == CSSSelector::kTag &&
                   selector.TagQName().LocalName() == tag_name &&
                   selector.TagQName().NamespaceURI() == g_star_atom;
          });
    }
    AddToRuleSet(tag_name, tag_rules_, rule_data);
    return;
  }

  // The selector parser prepends a :true pseudo-class with
  // relation=kScopeActivation to any compound that contains :scope
  // or the parent pseudo-class (&).
  if (component.Relation() == CSSSelector::kScopeActivation) {
    must_check_universal_bucket_for_shadow_host_ = true;
  }

  // Normally, rules involving :host would be stuck in their own bucket
  // above; if we came here, it is because we have something like :is(:host,
  // .foo). Mark that we have this case.
  if (component.IsOrContainsHostPseudoClass()) {
    must_check_universal_bucket_for_shadow_host_ = true;
  }

  // If we didn't find a specialized map to stick it in, file under universal
  // rules.
  MarkAsCoveredByBucketing(component, [](const CSSSelector& selector) {
    return selector.Match() == CSSSelector::kTag &&
           selector.TagQName() == AnyQName();
  });
  AddToRuleSet(universal_rules_, rule_data);
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
  RuleData rule_data(rule, selector_index, rule_count_, style_scope,
                     add_rule_flags, bloom_hash_backing_);
  ++rule_count_;
  {
    InvalidationSetToSelectorMap::SelectorScope selector_scope(rule,
                                                               selector_index);
    if (features_.CollectFeaturesFromSelector(rule_data.Selector(),
                                              style_scope) ==
        SelectorPreMatch::kNeverMatches) {
      return;
    }
  }

  FindBestRuleSetAndAdd<BucketCoverage::kCompute>(rule_data.MutableSelector(),
                                                  rule_data);

  // If the rule has CSSSelector::kMatchLink, it means that there is a :visited
  // or :link pseudo-class somewhere in the selector. In those cases, we
  // effectively split the rule into two: one which covers the situation
  // where we are in an unvisited link (kMatchLink), and another which covers
  // the visited link case (kMatchVisited).
  if (rule_data.LinkMatchType() == CSSSelector::kMatchLink) {
    // Now the selector will be in two buckets.
    rule_data.ResetEntirelyCoveredByBucketing();

    RuleData visited_dependent(
        rule, rule_data.SelectorIndex(), rule_data.GetPosition(), style_scope,
        add_rule_flags | kRuleIsVisitedDependent, bloom_hash_backing_);
    // Since the selector now is in two buckets, we use BucketCoverage::kIgnore
    // to prevent CSSSelector::is_covered_by_bucketing_ from being set.
    FindBestRuleSetAndAdd<BucketCoverage::kIgnore>(
        visited_dependent.MutableSelector(), visited_dependent);
  }

  AddRuleToLayerIntervals(cascade_layer, rule_data.GetPosition());
  AddRuleToIntervals(container_query, rule_data.GetPosition(),
                     container_query_intervals_);
  AddRuleToIntervals(style_scope, rule_data.GetPosition(), scope_intervals_);
}

void RuleSet::AddRuleToLayerIntervals(const CascadeLayer* cascade_layer,
                                      unsigned position) {
  // nullptr in this context means “no layer”, i.e., the implicit outer layer.
  if (!cascade_layer) {
    if (layer_intervals_.empty()) {
      // Don't create the implicit outer layer if we don't need to.
      return;
    } else {
      cascade_layer = EnsureImplicitOuterLayer();
    }
  }

  AddRuleToIntervals(cascade_layer, position, layer_intervals_);
}

// Similar to AddRuleToLayerIntervals, but for container queries and @style
// scopes.
template <class T>
static void AddRuleToIntervals(const T* value,
                               unsigned position,
                               HeapVector<RuleSet::Interval<T>>& intervals) {
  const T* last_value =
      intervals.empty() ? nullptr : intervals.back().value.Get();
  if (value == last_value) {
    return;
  }

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

void RuleSet::AddPositionTryRule(StyleRulePositionTry* rule) {
  need_compaction_ = true;
  position_try_rules_.push_back(rule);
}

void RuleSet::AddFunctionRule(StyleRuleFunction* rule) {
  need_compaction_ = true;
  function_rules_.push_back(rule);
}

void RuleSet::AddViewTransitionRule(StyleRuleViewTransition* rule) {
  need_compaction_ = true;
  view_transition_rules_.push_back(rule);
}

void RuleSet::AddChildRules(StyleRule* parent_rule,
                            const HeapVector<Member<StyleRuleBase>>& rules,
                            const MediaQueryEvaluator& medium,
                            AddRuleFlags add_rule_flags,
                            const ContainerQuery* container_query,
                            CascadeLayer* cascade_layer,
                            const StyleScope* style_scope,
                            bool within_mixin) {
  for (StyleRuleBase* rule : rules) {
    if (auto* style_rule = DynamicTo<StyleRule>(rule)) {
      AddStyleRule(style_rule, parent_rule, medium, add_rule_flags,
                   within_mixin, container_query, cascade_layer, style_scope);
    } else if (auto* page_rule = DynamicTo<StyleRulePage>(rule)) {
      page_rule->SetCascadeLayer(cascade_layer);
      AddPageRule(page_rule);
    } else if (auto* media_rule = DynamicTo<StyleRuleMedia>(rule)) {
      if (MatchMediaForAddRules(medium, media_rule->MediaQueries())) {
        AddChildRules(parent_rule, media_rule->ChildRules(), medium,
                      add_rule_flags, container_query, cascade_layer,
                      style_scope, within_mixin);
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
      font_feature_values_rule->SetCascadeLayer(cascade_layer);
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
    } else if (auto* view_transition_rule =
                   DynamicTo<StyleRuleViewTransition>(rule)) {
      view_transition_rule->SetCascadeLayer(cascade_layer);
      AddViewTransitionRule(view_transition_rule);
    } else if (auto* position_try_rule =
                   DynamicTo<StyleRulePositionTry>(rule)) {
      position_try_rule->SetCascadeLayer(cascade_layer);
      AddPositionTryRule(position_try_rule);
    } else if (auto* function_rule = DynamicTo<StyleRuleFunction>(rule)) {
      // TODO(sesse): Set the cascade layer here?
      AddFunctionRule(function_rule);
    } else if (auto* supports_rule = DynamicTo<StyleRuleSupports>(rule)) {
      if (supports_rule->ConditionIsSupported()) {
        AddChildRules(parent_rule, supports_rule->ChildRules(), medium,
                      add_rule_flags, container_query, cascade_layer,
                      style_scope, within_mixin);
      }
    } else if (auto* container_rule = DynamicTo<StyleRuleContainer>(rule)) {
      const ContainerQuery* inner_container_query =
          &container_rule->GetContainerQuery();
      if (container_query) {
        inner_container_query =
            inner_container_query->CopyWithParent(container_query);
      }
      AddChildRules(parent_rule, container_rule->ChildRules(), medium,
                    add_rule_flags, inner_container_query, cascade_layer,
                    style_scope, within_mixin);
    } else if (auto* layer_block_rule = DynamicTo<StyleRuleLayerBlock>(rule)) {
      CascadeLayer* sub_layer =
          GetOrAddSubLayer(cascade_layer, layer_block_rule->GetName());
      AddChildRules(parent_rule, layer_block_rule->ChildRules(), medium,
                    add_rule_flags, container_query, sub_layer, style_scope,
                    within_mixin);
    } else if (auto* layer_statement_rule =
                   DynamicTo<StyleRuleLayerStatement>(rule)) {
      for (const auto& layer_name : layer_statement_rule->GetNames()) {
        GetOrAddSubLayer(cascade_layer, layer_name);
      }
    } else if (auto* scope_rule = DynamicTo<StyleRuleScope>(rule)) {
      const StyleScope* inner_style_scope = &scope_rule->GetStyleScope();
      if (style_scope) {
        inner_style_scope = inner_style_scope->CopyWithParent(style_scope);
      }
      AddChildRules(parent_rule, scope_rule->ChildRules(), medium,
                    add_rule_flags, container_query, cascade_layer,
                    inner_style_scope, within_mixin);
    } else if (auto* starting_style_rule =
                   DynamicTo<StyleRuleStartingStyle>(rule)) {
      AddChildRules(parent_rule, starting_style_rule->ChildRules(), medium,
                    add_rule_flags | kRuleIsStartingStyle, container_query,
                    cascade_layer, style_scope, within_mixin);
    } else if (auto* mixin_rule = DynamicTo<StyleRuleMixin>(rule)) {
      mixins_.Set(mixin_rule->GetName(), mixin_rule);
    } else if (auto* apply_mixin_rule = DynamicTo<StyleRuleApplyMixin>(rule)) {
      // TODO(sesse): This lookup needs to work completely differently
      // if we are to support mixins from different stylesheets.
      // In particular, we need to implement tree-scoped lookups
      // in a situation where we don't have the normal ScopedStyleResolver
      // available, and also take into account that sharing RuleSets
      // won't really work if we cross-reference mixins from other sheets.
      auto it = mixins_.find(apply_mixin_rule->GetName());
      if (it != mixins_.end() && it->value->FakeParentRule().ChildRules()) {
        AddChildRules(parent_rule, *it->value->FakeParentRule().ChildRules(),
                      medium, add_rule_flags, container_query, cascade_layer,
                      style_scope, /*within_mixin=*/true);
      }
    } else if (auto* nested_declarations =
                   DynamicTo<StyleRuleNestedDeclarations>(rule)) {
      AddStyleRule(nested_declarations->InnerStyleRule(), parent_rule, medium,
                   add_rule_flags, within_mixin, container_query, cascade_layer,
                   style_scope);
    }
  }
}

bool RuleSet::MatchMediaForAddRules(const MediaQueryEvaluator& evaluator,
                                    const MediaQuerySet* media_queries) {
  if (!media_queries) {
    return true;
  }
  bool match_media =
      evaluator.Eval(*media_queries, &features_.MutableMediaQueryResultFlags());
  media_query_set_results_.push_back(
      MediaQuerySetResult(*media_queries, match_media));
  return match_media;
}

void RuleSet::AddRulesFromSheet(StyleSheetContents* sheet,
                                const MediaQueryEvaluator& medium,
                                CascadeLayer* cascade_layer) {
  TRACE_EVENT0("blink", "RuleSet::addRulesFromSheet");
  DCHECK(sheet);

  for (const auto& pre_import_layer : sheet->PreImportLayerStatementRules()) {
    for (const auto& name : pre_import_layer->GetNames()) {
      GetOrAddSubLayer(cascade_layer, name);
    }
  }

  const HeapVector<Member<StyleRuleImport>>& import_rules =
      sheet->ImportRules();
  for (unsigned i = 0; i < import_rules.size(); ++i) {
    StyleRuleImport* import_rule = import_rules[i].Get();
    if (!import_rule->IsSupported()) {
      continue;
    }
    if (!MatchMediaForAddRules(medium, import_rule->MediaQueries())) {
      continue;
    }
    CascadeLayer* import_layer = cascade_layer;
    if (import_rule->IsLayered()) {
      import_layer =
          GetOrAddSubLayer(cascade_layer, import_rule->GetLayerName());
    }
    if (import_rule->GetStyleSheet()) {
      AddRulesFromSheet(import_rule->GetStyleSheet(), medium, import_layer);
    }
  }

  AddChildRules(/*parent_rule=*/nullptr, sheet->ChildRules(), medium,
                kRuleHasNoSpecialState, nullptr /* container_query */,
                cascade_layer, nullptr, /*within_mixin=*/false);
}

// If there's a reference to the parent selector (implicit or explicit)
// somewhere in the selector, use that to find the parent StyleRule.
// If not, it's not relevant what the parent is anyway.
const StyleRule* FindParentIfUsed(const CSSSelector* selector) {
  do {
    if (selector->Match() == CSSSelector::kPseudoClass &&
        selector->GetPseudoType() == CSSSelector::kPseudoParent) {
      return selector->ParentRule();
    }
    if (selector->SelectorList() && selector->SelectorList()->First()) {
      const StyleRule* parent =
          FindParentIfUsed(selector->SelectorList()->First());
      if (parent != nullptr) {
        return parent;
      }
    }
  } while (!(selector++)->IsLastInSelectorList());
  return nullptr;
}

// Whether we should include the given rule (coming from a RuleSet)
// in a diff rule set, based on the list on “only_include” (which are
// the ones that have been modified). This is nominally only a simple
// membership test, but we also need to take into account nested rules;
// if a parent rule of ours has been modified, we need to also include
// this rule.
static bool IncludeRule(const StyleRule* style_rule,
                        const HeapHashSet<Member<StyleRule>>& only_include) {
  if (only_include.Contains(const_cast<StyleRule*>(style_rule))) {
    return true;
  }
  const StyleRule* parent_rule = FindParentIfUsed(style_rule->FirstSelector());
  if (parent_rule != nullptr) {
    return IncludeRule(parent_rule, only_include);
  } else {
    return false;
  }
}

void RuleSet::NewlyAddedFromDifferentRuleSet(const RuleData& old_rule_data,
                                             const StyleScope* style_scope,
                                             const RuleSet& old_rule_set,
                                             RuleData& new_rule_data) {
  new_rule_data.MovedToDifferentRuleSet(old_rule_set.bloom_hash_backing_,
                                        bloom_hash_backing_, rule_count_);
  // We don't bother with container_query_intervals_ and
  // AddRuleToLayerIntervals() here, since they are not checked in diff
  // rulesets.
  AddRuleToIntervals(style_scope, rule_count_, scope_intervals_);
  ++rule_count_;
}

void RuleSet::AddFilteredRulesFromOtherBucket(
    const RuleSet& other,
    const HeapVector<RuleData>& src,
    const HeapHashSet<Member<StyleRule>>& only_include,
    HeapVector<RuleData>* dst) {
  Seeker<StyleScope> scope_seeker(other.scope_intervals_);
  for (const RuleData& rule_data : src) {
    if (IncludeRule(rule_data.Rule(), only_include)) {
      dst->push_back(rule_data);
      NewlyAddedFromDifferentRuleSet(rule_data,
                                     scope_seeker.Seek(rule_data.GetPosition()),
                                     other, dst->back());
    }
  }
}

void RuleSet::AddFilteredRulesFromOtherSet(
    const RuleSet& other,
    const HeapHashSet<Member<StyleRule>>& only_include) {
  if (other.rule_count_ > 0) {
    id_rules_.AddFilteredRulesFromOtherSet(other.id_rules_, only_include, other,
                                           *this);
    class_rules_.AddFilteredRulesFromOtherSet(other.class_rules_, only_include,
                                              other, *this);
    attr_rules_.AddFilteredRulesFromOtherSet(other.attr_rules_, only_include,
                                             other, *this);
    // NOTE: attr_substring_matchers_ will be rebuilt in CompactRules().
    tag_rules_.AddFilteredRulesFromOtherSet(other.tag_rules_, only_include,
                                            other, *this);
    ua_shadow_pseudo_element_rules_.AddFilteredRulesFromOtherSet(
        other.ua_shadow_pseudo_element_rules_, only_include, other, *this);
    AddFilteredRulesFromOtherBucket(other, other.link_pseudo_class_rules_,
                                    only_include, &link_pseudo_class_rules_);
    AddFilteredRulesFromOtherBucket(other, other.cue_pseudo_rules_,
                                    only_include, &cue_pseudo_rules_);
    AddFilteredRulesFromOtherBucket(other, other.focus_pseudo_class_rules_,
                                    only_include, &focus_pseudo_class_rules_);
    AddFilteredRulesFromOtherBucket(
        other, other.focus_visible_pseudo_class_rules_, only_include,
        &focus_visible_pseudo_class_rules_);
    AddFilteredRulesFromOtherBucket(other, other.universal_rules_, only_include,
                                    &universal_rules_);
    AddFilteredRulesFromOtherBucket(other, other.shadow_host_rules_,
                                    only_include, &shadow_host_rules_);
    AddFilteredRulesFromOtherBucket(other, other.part_pseudo_rules_,
                                    only_include, &part_pseudo_rules_);
    AddFilteredRulesFromOtherBucket(other, other.slotted_pseudo_element_rules_,
                                    only_include,
                                    &slotted_pseudo_element_rules_);
    AddFilteredRulesFromOtherBucket(
        other, other.selector_fragment_anchor_rules_, only_include,
        &selector_fragment_anchor_rules_);
    AddFilteredRulesFromOtherBucket(other, other.root_element_rules_,
                                    only_include, &root_element_rules_);

    // We don't care about page_rules_ etc., since having those in a RuleSetDiff
    // would mark it as unrepresentable anyway.

    need_compaction_ = true;
  }

#if EXPENSIVE_DCHECKS_ARE_ON()
  allow_unsorted_ = true;
#endif
}

void RuleSet::AddStyleRule(StyleRule* style_rule,
                           StyleRule* parent_rule,
                           const MediaQueryEvaluator& medium,
                           AddRuleFlags add_rule_flags,
                           bool within_mixin,
                           const ContainerQuery* container_query,
                           CascadeLayer* cascade_layer,
                           const StyleScope* style_scope) {
  if (within_mixin) {
    style_rule = style_rule->Copy();
    style_rule->Reparent(parent_rule);
  }
  for (const CSSSelector* selector = style_rule->FirstSelector(); selector;
       selector = CSSSelectorList::Next(*selector)) {
    wtf_size_t selector_index = style_rule->SelectorIndex(*selector);
    AddRule(style_rule, selector_index, add_rule_flags, container_query,
            cascade_layer, style_scope);
  }

  // Nested rules are taken to be added immediately after their parent rule.
  if (style_rule->ChildRules() != nullptr) {
    AddChildRules(style_rule, *style_rule->ChildRules(), medium, add_rule_flags,
                  container_query, cascade_layer, style_scope, within_mixin);
  }
}

CascadeLayer* RuleSet::GetOrAddSubLayer(CascadeLayer* cascade_layer,
                                        const StyleRuleBase::LayerName& name) {
  if (!cascade_layer) {
    cascade_layer = EnsureImplicitOuterLayer();
  }
  return cascade_layer->GetOrAddSubLayer(name);
}

bool RuleMap::Add(const AtomicString& key, const RuleData& rule_data) {
  RuleMap::Extent* rules = nullptr;
  if (buckets.IsNull()) {
    // First insert.
    buckets = RobinHoodMap<AtomicString, Extent>(8);
  } else {
    // See if we can find an existing entry for this key.
    RobinHoodMap<AtomicString, Extent>::Bucket* bucket = buckets.Find(key);
    if (bucket != nullptr) {
      rules = &bucket->value;
    }
  }
  if (rules == nullptr) {
    RobinHoodMap<AtomicString, Extent>::Bucket* bucket = buckets.Insert(key);
    if (bucket == nullptr) {
      return false;
    }
    rules = &bucket->value;
    rules->bucket_number = num_buckets++;
  }

  RuleData rule_data_copy = rule_data;
  rule_data_copy.ComputeEntirelyCoveredByBucketing();
  bucket_number_.push_back(rules->bucket_number);
  ++rules->length;
  backing.push_back(std::move(rule_data_copy));
  return true;
}

void RuleMap::Compact() {
  if (compacted) {
    return;
  }
  if (backing.empty()) {
    DCHECK(bucket_number_.empty());
    // Nothing to do.
    compacted = true;
    return;
  }

  backing.shrink_to_fit();

  // Order by (bucket_number, order_in_bucket) by way of a simple
  // in-place counting sort (which is O(n), because our highest bucket
  // number is always less than or equal to the number of elements).
  // First, we make an array that contains the number of elements in each
  // bucket, indexed by the bucket number. We also find each element's
  // position within that bucket.
  auto counts =
      base::HeapArray<unsigned>::WithSize(num_buckets);  // Zero-initialized.
  auto order_in_bucket = base::HeapArray<unsigned>::Uninit(backing.size());
  for (wtf_size_t i = 0; i < bucket_number_.size(); ++i) {
    order_in_bucket[i] = counts[bucket_number_[i]]++;
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
    wtf_size_t correct_pos = counts[bucket_number_[i]] + order_in_bucket[i];
    if (i == correct_pos) {
      ++i;
    } else {
      using std::swap;
      swap(backing[i], backing[correct_pos]);
      swap(bucket_number_[i], bucket_number_[correct_pos]);
      swap(order_in_bucket[i], order_in_bucket[correct_pos]);
    }
  }

  // We're done with the bucket numbers, so we can release the memory.
  // If we need the bucket numbers again, they will be reconstructed by
  // RuleMap::Uncompact.
  bucket_number_.clear();

  compacted = true;
}

void RuleMap::Uncompact() {
  bucket_number_.resize(backing.size());

  num_buckets = 0;
  for (auto& [key, value] : buckets) {
    for (unsigned& bucket_number : GetBucketNumberFromExtent(value)) {
      bucket_number = num_buckets;
    }
    value.bucket_number = num_buckets++;
    value.length =
        static_cast<unsigned>(GetBucketNumberFromExtent(value).size());
  }
  compacted = false;
}

// See RuleSet::AddFilteredRulesFromOtherSet().
void RuleMap::AddFilteredRulesFromOtherSet(
    const RuleMap& other,
    const HeapHashSet<Member<StyleRule>>& only_include,
    const RuleSet& old_rule_set,
    RuleSet& new_rule_set) {
  if (compacted) {
    Uncompact();
  }
  if (other.compacted) {
    for (const auto& [key, extent] : other.buckets) {
      Seeker<StyleScope> scope_seeker(old_rule_set.scope_intervals_);
      for (const RuleData& rule_data : other.GetRulesFromExtent(extent)) {
        if (IncludeRule(rule_data.Rule(), only_include)) {
          Add(key, rule_data);
          new_rule_set.NewlyAddedFromDifferentRuleSet(
              rule_data, scope_seeker.Seek(rule_data.GetPosition()),
              old_rule_set, backing.back());
        }
      }
    }
  } else {
    // First make a mapping of bucket number to key.
    auto keys = base::HeapArray<const AtomicString*>::Uninit(other.num_buckets);
    for (const auto& [key, src_extent] : other.buckets) {
      keys[src_extent.bucket_number] = &key;
    }

    // Now that we have the mapping, we can just copy over all the relevant
    // RuleDatas.
    Seeker<StyleScope> scope_seeker(old_rule_set.scope_intervals_);
    for (wtf_size_t i = 0; i < other.backing.size(); ++i) {
      const unsigned bucket_number = other.bucket_number_[i];
      const RuleData& rule_data = other.backing[i];
      if (IncludeRule(rule_data.Rule(), only_include)) {
        Add(*keys[bucket_number], rule_data);
        new_rule_set.NewlyAddedFromDifferentRuleSet(
            rule_data, scope_seeker.Seek(rule_data.GetPosition()), old_rule_set,
            backing.back());
      }
    }
  }
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
    DCHECK(!base::Contains(attr_substring_matchers_, key));
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
      AtomicString picker_name;
      bool is_exact_attr;
      CSSSelector::PseudoType pseudo_type = CSSSelector::kPseudoUnknown;
      ExtractBestSelectorValues(rule.Selector(), id, class_name, attr_name,
                                attr_value, is_exact_attr,
                                custom_pseudo_element_name, tag_name, part_name,
                                picker_name, pseudo_type);
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
  universal_rules_.shrink_to_fit();
  shadow_host_rules_.shrink_to_fit();
  part_pseudo_rules_.shrink_to_fit();
  slotted_pseudo_element_rules_.shrink_to_fit();
  page_rules_.shrink_to_fit();
  font_face_rules_.shrink_to_fit();
  font_palette_values_rules_.shrink_to_fit();
  keyframes_rules_.shrink_to_fit();
  property_rules_.shrink_to_fit();
  counter_style_rules_.shrink_to_fit();
  position_try_rules_.shrink_to_fit();
  layer_intervals_.shrink_to_fit();
  view_transition_rules_.shrink_to_fit();
  bloom_hash_backing_.shrink_to_fit();

#if EXPENSIVE_DCHECKS_ARE_ON()
  if (!allow_unsorted_) {
    AssertRuleListsSorted();
  }
#endif
  need_compaction_ = false;
}

#if EXPENSIVE_DCHECKS_ARE_ON()

namespace {

// Rules that depend on visited link status may be added twice to the same
// bucket (with different LinkMatchTypes).
bool AllowSamePosition(const RuleData& current, const RuleData& previous) {
  return current.LinkMatchType() != previous.LinkMatchType();
}

template <class RuleList>
bool IsRuleListSorted(const RuleList& rules) {
  const RuleData* last_rule = nullptr;
  for (const RuleData& rule : rules) {
    if (last_rule) {
      if (rule.GetPosition() == last_rule->GetPosition()) {
        if (!AllowSamePosition(rule, *last_rule)) {
          return false;
        }
      }
      if (rule.GetPosition() < last_rule->GetPosition()) {
        return false;
      }
    }
    last_rule = &rule;
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
  DCHECK(IsRuleListSorted(universal_rules_));
  DCHECK(IsRuleListSorted(shadow_host_rules_));
  DCHECK(IsRuleListSorted(part_pseudo_rules_));
}

#endif  // EXPENSIVE_DCHECKS_ARE_ON()

bool RuleSet::DidMediaQueryResultsChange(
    const MediaQueryEvaluator& evaluator) const {
  return evaluator.DidResultsChange(media_query_set_results_);
}

const CascadeLayer* RuleSet::GetLayerForTest(const RuleData& rule) const {
  if (!layer_intervals_.size() ||
      layer_intervals_[0].start_position > rule.GetPosition()) {
    return implicit_outer_layer_.Get();
  }
  for (unsigned i = 1; i < layer_intervals_.size(); ++i) {
    if (layer_intervals_[i].start_position > rule.GetPosition()) {
      return layer_intervals_[i - 1].value.Get();
    }
  }
  return layer_intervals_.back().value.Get();
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
  visitor->Trace(universal_rules_);
  visitor->Trace(shadow_host_rules_);
  visitor->Trace(part_pseudo_rules_);
  visitor->Trace(slotted_pseudo_element_rules_);
  visitor->Trace(page_rules_);
  visitor->Trace(font_face_rules_);
  visitor->Trace(font_palette_values_rules_);
  visitor->Trace(font_feature_values_rules_);
  visitor->Trace(view_transition_rules_);
  visitor->Trace(keyframes_rules_);
  visitor->Trace(property_rules_);
  visitor->Trace(counter_style_rules_);
  visitor->Trace(position_try_rules_);
  visitor->Trace(function_rules_);
  visitor->Trace(root_element_rules_);
  visitor->Trace(media_query_set_results_);
  visitor->Trace(implicit_outer_layer_);
  visitor->Trace(layer_intervals_);
  visitor->Trace(container_query_intervals_);
  visitor->Trace(scope_intervals_);
  visitor->Trace(mixins_);
#if DCHECK_IS_ON()
  visitor->Trace(all_rules_);
#endif  // DCHECK_IS_ON()
}

#if DCHECK_IS_ON()
void RuleSet::Show() const {
  for (const RuleData& rule : all_rules_) {
    rule.Selector().Show();
  }
}
#endif  // DCHECK_IS_ON()

}  // namespace blink
