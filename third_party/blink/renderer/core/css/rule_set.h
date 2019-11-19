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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RULE_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RULE_SET_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/resolver/media_query_result.h"
#include "third_party/blink/renderer/core/css/rule_feature_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/platform/heap/heap_linked_stack.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

enum AddRuleFlags {
  kRuleHasNoSpecialState = 0,
  kRuleHasDocumentSecurityOrigin = 1,
};

// Some CSS properties do not apply to certain pseudo-elements, and need to be
// ignored when resolving styles.
enum class ValidPropertyFilter : unsigned {
  // All properties are valid. This is the common case.
  kNoFilter,
  // Defined in a ::cue pseudo-element scope. Only properties listed
  // in https://w3c.github.io/webvtt/#the-cue-pseudo-element are valid.
  kCue,
  // Defined in a ::first-letter pseudo-element scope. Only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#first-letter-styling are valid.
  kFirstLetter,
  // Defined in a ::marker pseudo-element scope. Only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#marker-pseudo are valid.
  kMarker,
};

class CSSSelector;
class MediaQueryEvaluator;
class StyleSheetContents;

class MinimalRuleData {
  DISALLOW_NEW();

 public:
  MinimalRuleData(StyleRule* rule, unsigned selector_index, AddRuleFlags flags)
      : rule_(rule), selector_index_(selector_index), flags_(flags) {}

  void Trace(blink::Visitor*);

  Member<StyleRule> rule_;
  unsigned selector_index_;
  AddRuleFlags flags_;
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::MinimalRuleData)

namespace blink {

// This is a wrapper around a StyleRule, pointing to one of the N complex
// selectors in the StyleRule. This allows us to treat each selector
// independently but still tie them back to the original StyleRule. If multiple
// selectors from a single rule match the same element we can see that as one
// match for the rule. It computes some information about the wrapped selector
// and makes it accessible cheaply.
class CORE_EXPORT RuleData : public GarbageCollected<RuleData> {
 public:
  static RuleData* MaybeCreate(StyleRule*,
                               unsigned selector_index,
                               unsigned position,
                               AddRuleFlags);

  RuleData(StyleRule*,
           unsigned selector_index,
           unsigned position,
           AddRuleFlags);

  unsigned GetPosition() const { return position_; }
  StyleRule* Rule() const { return rule_; }
  const CSSSelector& Selector() const {
    return rule_->SelectorList().SelectorAt(selector_index_);
  }
  unsigned SelectorIndex() const { return selector_index_; }

  bool ContainsUncommonAttributeSelector() const {
    return contains_uncommon_attribute_selector_;
  }
  unsigned Specificity() const { return specificity_; }
  unsigned LinkMatchType() const { return link_match_type_; }
  bool HasDocumentSecurityOrigin() const {
    return has_document_security_origin_;
  }
  ValidPropertyFilter GetValidPropertyFilter(
      bool is_matching_ua_rules = false) const {
    return is_matching_ua_rules
               ? ValidPropertyFilter::kNoFilter
               : static_cast<ValidPropertyFilter>(valid_property_filter_);
  }
  // Try to balance between memory usage (there can be lots of RuleData objects)
  // and good filtering performance.
  static const unsigned kMaximumIdentifierCount = 4;
  const unsigned* DescendantSelectorIdentifierHashes() const {
    return descendant_selector_identifier_hashes_;
  }

  void Trace(blink::Visitor*);

  // This number is picked fairly arbitrary. If lowered, be aware that there
  // might be sites and extensions using style rules with selector lists
  // exceeding the number of simple selectors to fit in this bitfield.
  // See https://crbug.com/312913 and https://crbug.com/704562
  static constexpr size_t kSelectorIndexBits = 13;

  // This number was picked fairly arbitrarily. We can probably lower it if we
  // need to. Some simple testing showed <100,000 RuleData's on large sites.
  static constexpr size_t kPositionBits = 18;

 private:
  Member<StyleRule> rule_;
  unsigned selector_index_ : kSelectorIndexBits;
  unsigned position_ : kPositionBits;
  unsigned contains_uncommon_attribute_selector_ : 1;
  // 32 bits above
  unsigned specificity_ : 24;
  unsigned link_match_type_ : 2;  //  CSSSelector::LinkMatchMask
  unsigned has_document_security_origin_ : 1;
  unsigned valid_property_filter_ : 2;
  // 29 bits above
  // Use plain array instead of a Vector to minimize memory overhead.
  unsigned descendant_selector_identifier_hashes_[kMaximumIdentifierCount];
};

}  // namespace blink

WTF_ALLOW_MOVE_AND_INIT_WITH_MEM_FUNCTIONS(blink::RuleData)

namespace blink {

struct SameSizeAsRuleData {
  DISALLOW_NEW();
  Member<void*> a;
  unsigned b;
  unsigned c;
  unsigned d[4];
};

static_assert(sizeof(RuleData) == sizeof(SameSizeAsRuleData),
              "RuleData should stay small");

// Holds RuleData objects. It partitions them into various indexed groups,
// e.g. it stores separately rules that match against id, class, tag, shadow
// host, etc. It indexes these by some key where possible, e.g. rules that match
// against tag name are indexed by that tag. Rules that don't fall into a
// specific group are appended to the "universal" rules. The grouping is done to
// optimize finding what rules apply to an element under consideration by
// ElementRuleCollector::CollectMatchingRules.
class CORE_EXPORT RuleSet final : public GarbageCollected<RuleSet> {
 public:
  RuleSet() : rule_count_(0) {}

  void AddRulesFromSheet(StyleSheetContents*,
                         const MediaQueryEvaluator&,
                         AddRuleFlags = kRuleHasNoSpecialState);
  void AddStyleRule(StyleRule*, AddRuleFlags);
  void AddRule(StyleRule*, unsigned selector_index, AddRuleFlags);

  const RuleFeatureSet& Features() const { return features_; }

  const HeapVector<Member<const RuleData>>* IdRules(
      const AtomicString& key) const {
    DCHECK(!pending_rules_);
    return id_rules_.at(key);
  }
  const HeapVector<Member<const RuleData>>* ClassRules(
      const AtomicString& key) const {
    DCHECK(!pending_rules_);
    return class_rules_.at(key);
  }
  const HeapVector<Member<const RuleData>>* TagRules(
      const AtomicString& key) const {
    DCHECK(!pending_rules_);
    return tag_rules_.at(key);
  }
  const HeapVector<Member<const RuleData>>* ShadowPseudoElementRules(
      const AtomicString& key) const {
    DCHECK(!pending_rules_);
    return shadow_pseudo_element_rules_.at(key);
  }
  const HeapVector<Member<const RuleData>>* LinkPseudoClassRules() const {
    DCHECK(!pending_rules_);
    return &link_pseudo_class_rules_;
  }
  const HeapVector<Member<const RuleData>>* CuePseudoRules() const {
    DCHECK(!pending_rules_);
    return &cue_pseudo_rules_;
  }
  const HeapVector<Member<const RuleData>>* FocusPseudoClassRules() const {
    DCHECK(!pending_rules_);
    return &focus_pseudo_class_rules_;
  }
  const HeapVector<Member<const RuleData>>*
  SpatialNavigationInterestPseudoClassRules() const {
    DCHECK(!pending_rules_);
    return &spatial_navigation_interest_class_rules_;
  }
  const HeapVector<Member<const RuleData>>* UniversalRules() const {
    DCHECK(!pending_rules_);
    return &universal_rules_;
  }
  const HeapVector<Member<const RuleData>>* ShadowHostRules() const {
    DCHECK(!pending_rules_);
    return &shadow_host_rules_;
  }
  const HeapVector<Member<const RuleData>>* PartPseudoRules() const {
    DCHECK(!pending_rules_);
    return &part_pseudo_rules_;
  }
  const HeapVector<Member<StyleRulePage>>& PageRules() const {
    DCHECK(!pending_rules_);
    return page_rules_;
  }
  const HeapVector<Member<StyleRuleFontFace>>& FontFaceRules() const {
    return font_face_rules_;
  }
  const HeapVector<Member<StyleRuleKeyframes>>& KeyframesRules() const {
    return keyframes_rules_;
  }
  const HeapVector<Member<StyleRuleProperty>>& PropertyRules() const {
    return property_rules_;
  }
  const HeapVector<MinimalRuleData>& DeepCombinatorOrShadowPseudoRules() const {
    return deep_combinator_or_shadow_pseudo_rules_;
  }
  const HeapVector<MinimalRuleData>& ContentPseudoElementRules() const {
    return content_pseudo_element_rules_;
  }
  const HeapVector<MinimalRuleData>& SlottedPseudoElementRules() const {
    return slotted_pseudo_element_rules_;
  }

  unsigned RuleCount() const { return rule_count_; }

  void CompactRulesIfNeeded() {
    if (!pending_rules_)
      return;
    CompactRules();
  }

  bool HasSlottedRules() const {
    return !slotted_pseudo_element_rules_.IsEmpty();
  }

  bool HasV0BoundaryCrossingRules() const {
    return !deep_combinator_or_shadow_pseudo_rules_.IsEmpty() ||
           !content_pseudo_element_rules_.IsEmpty();
  }

  bool NeedsFullRecalcForRuleSetInvalidation() const {
    return features_.NeedsFullRecalcForRuleSetInvalidation() ||
           HasV0BoundaryCrossingRules();
  }

#ifndef NDEBUG
  void Show() const;
#endif

  void Trace(blink::Visitor*);

 private:
  using PendingRuleMap =
      HeapHashMap<AtomicString,
                  Member<HeapLinkedStack<Member<const RuleData>>>>;
  using CompactRuleMap =
      HeapHashMap<AtomicString, Member<HeapVector<Member<const RuleData>>>>;

  void AddToRuleSet(const AtomicString& key, PendingRuleMap&, const RuleData*);
  void AddPageRule(StyleRulePage*);
  void AddViewportRule(StyleRuleViewport*);
  void AddFontFaceRule(StyleRuleFontFace*);
  void AddKeyframesRule(StyleRuleKeyframes*);
  void AddPropertyRule(StyleRuleProperty*);

  void AddChildRules(const HeapVector<Member<StyleRuleBase>>&,
                     const MediaQueryEvaluator& medium,
                     AddRuleFlags);
  bool FindBestRuleSetAndAdd(const CSSSelector&, RuleData*);

  void CompactRules();
  static void CompactPendingRules(PendingRuleMap&, CompactRuleMap&);

  class PendingRuleMaps : public GarbageCollected<PendingRuleMaps> {
   public:
    PendingRuleMaps() = default;

    PendingRuleMap id_rules;
    PendingRuleMap class_rules;
    PendingRuleMap tag_rules;
    PendingRuleMap shadow_pseudo_element_rules;

    void Trace(blink::Visitor*);
  };

  PendingRuleMaps* EnsurePendingRules() {
    if (!pending_rules_)
      pending_rules_ = MakeGarbageCollected<PendingRuleMaps>();
    return pending_rules_.Get();
  }

  CompactRuleMap id_rules_;
  CompactRuleMap class_rules_;
  CompactRuleMap tag_rules_;
  CompactRuleMap shadow_pseudo_element_rules_;
  HeapVector<Member<const RuleData>> link_pseudo_class_rules_;
  HeapVector<Member<const RuleData>> cue_pseudo_rules_;
  HeapVector<Member<const RuleData>> focus_pseudo_class_rules_;
  HeapVector<Member<const RuleData>> spatial_navigation_interest_class_rules_;
  HeapVector<Member<const RuleData>> universal_rules_;
  HeapVector<Member<const RuleData>> shadow_host_rules_;
  HeapVector<Member<const RuleData>> part_pseudo_rules_;
  RuleFeatureSet features_;
  HeapVector<Member<StyleRulePage>> page_rules_;
  HeapVector<Member<StyleRuleFontFace>> font_face_rules_;
  HeapVector<Member<StyleRuleKeyframes>> keyframes_rules_;
  HeapVector<Member<StyleRuleProperty>> property_rules_;
  HeapVector<MinimalRuleData> deep_combinator_or_shadow_pseudo_rules_;
  HeapVector<MinimalRuleData> content_pseudo_element_rules_;
  HeapVector<MinimalRuleData> slotted_pseudo_element_rules_;

  unsigned rule_count_;
  Member<PendingRuleMaps> pending_rules_;

#ifndef NDEBUG
  HeapVector<Member<const RuleData>> all_rules_;
#endif
  DISALLOW_COPY_AND_ASSIGN(RuleSet);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RULE_SET_H_
