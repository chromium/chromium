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

#include "base/substring_set_matcher/substring_set_matcher.h"
#include "base/types/pass_key.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/cascade_layer.h"
#include "third_party/blink/renderer/core/css/css_keyframes_rule.h"
#include "third_party/blink/renderer/core/css/css_position_fallback_rule.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/core/css/resolver/media_query_result.h"
#include "third_party/blink/renderer/core/css/rule_feature_set.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/css/style_rule_counter_style.h"
#include "third_party/blink/renderer/core/css/style_rule_font_palette_values.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_stack.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

using AddRuleFlags = unsigned;

enum AddRuleFlag {
  kRuleHasNoSpecialState = 0,
  kRuleHasDocumentSecurityOrigin = 1 << 0,
  kRuleIsVisitedDependent = 1 << 1,
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
  // Defined in a ::first-line pseudo-element scope. Only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#first-line-styling are valid.
  kFirstLine,
  // Defined in a ::marker pseudo-element scope. Only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#marker-pseudo are valid.
  kMarker,
  // Defined in a highlight pseudo-element scope like ::selection and
  // ::target-text. Theoretically only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#highlight-styling should be valid,
  // but for highlight pseudos using originating inheritance instead of
  // highlight inheritance we allow a different set of rules for
  // compatibility reasons.
  kHighlightLegacy,
  // Defined in a highlight pseudo-element scope like ::selection and
  // ::target-text. Only properties listed in
  // https://drafts.csswg.org/css-pseudo-4/#highlight-styling are valid.
  kHighlight,
};

class CSSSelector;
class MediaQueryEvaluator;
class StyleSheetContents;

// This is a wrapper around a StyleRule, pointing to one of the N complex
// selectors in the StyleRule. This allows us to treat each selector
// independently but still tie them back to the original StyleRule. If multiple
// selectors from a single rule match the same element we can see that as one
// match for the rule. It computes some information about the wrapped selector
// and makes it accessible cheaply.
class CORE_EXPORT RuleData {
  DISALLOW_NEW();

 public:
  // The `extra_specificity` parameter is added to the specificity of the
  // RuleData. This is useful for @scope, where inner selectors must gain
  // additional specificity from the <scope-start> of the enclosing @scope.
  // https://drafts.csswg.org/css-cascade-6/#scope-atrule
  RuleData(StyleRule*,
           unsigned selector_index,
           unsigned position,
           unsigned extra_specificity,
           AddRuleFlags);

  unsigned GetPosition() const { return position_; }
  StyleRule* Rule() const { return rule_; }
  const CSSSelector& Selector() const {
    return rule_->SelectorAt(selector_index_);
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

  void Trace(Visitor*) const;

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
  unsigned link_match_type_ : 2;
  unsigned has_document_security_origin_ : 1;
  unsigned valid_property_filter_ : 3;
  // 30 bits above
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

ASSERT_SIZE(RuleData, SameSizeAsRuleData);

// Holds RuleData objects. It partitions them into various indexed groups,
// e.g. it stores separately rules that match against id, class, tag, shadow
// host, etc. It indexes these by some key where possible, e.g. rules that match
// against tag name are indexed by that tag. Rules that don't fall into a
// specific group are appended to the "universal" rules. The grouping is done to
// optimize finding what rules apply to an element under consideration by
// ElementRuleCollector::CollectMatchingRules.
class CORE_EXPORT RuleSet final : public GarbageCollected<RuleSet> {
 public:
  RuleSet() = default;
  RuleSet(const RuleSet&) = delete;
  RuleSet& operator=(const RuleSet&) = delete;

  void AddRulesFromSheet(StyleSheetContents*,
                         const MediaQueryEvaluator&,
                         AddRuleFlags = kRuleHasNoSpecialState,
                         CascadeLayer* = nullptr);
  void AddStyleRule(StyleRule*, AddRuleFlags);

  const RuleFeatureSet& Features() const { return features_; }

  const HeapVector<RuleData>* IdRules(const AtomicString& key) const {
    auto it = id_rules_.find(key);
    return it != id_rules_.end() ? it->value : nullptr;
  }
  const HeapVector<RuleData>* ClassRules(const AtomicString& key) const {
    auto it = class_rules_.find(key);
    return it != class_rules_.end() ? it->value : nullptr;
  }
  bool HasAnyAttrRules() const { return !attr_rules_.IsEmpty(); }
  const HeapVector<RuleData>* AttrRules(const AtomicString& key) const {
    auto it = attr_rules_.find(key);
    return it != attr_rules_.end() ? it->value : nullptr;
  }
  bool CanIgnoreEntireList(const HeapVector<RuleData>* list,
                           const AtomicString& key,
                           const AtomicString& value) const;
  const HeapVector<RuleData>* TagRules(const AtomicString& key) const {
    auto it = tag_rules_.find(key);
    return it != tag_rules_.end() ? it->value : nullptr;
  }
  const HeapVector<RuleData>* UAShadowPseudoElementRules(
      const AtomicString& key) const {
    auto it = ua_shadow_pseudo_element_rules_.find(key);
    return it != ua_shadow_pseudo_element_rules_.end() ? it->value : nullptr;
  }
  const HeapVector<RuleData>* LinkPseudoClassRules() const {
    return &link_pseudo_class_rules_;
  }
  const HeapVector<RuleData>* CuePseudoRules() const {
    return &cue_pseudo_rules_;
  }
  const HeapVector<RuleData>* FocusPseudoClassRules() const {
    return &focus_pseudo_class_rules_;
  }
  const HeapVector<RuleData>* FocusVisiblePseudoClassRules() const {
    return &focus_visible_pseudo_class_rules_;
  }
  const HeapVector<RuleData>* SpatialNavigationInterestPseudoClassRules()
      const {
    return &spatial_navigation_interest_class_rules_;
  }
  const HeapVector<RuleData>* UniversalRules() const {
    return &universal_rules_;
  }
  const HeapVector<RuleData>* ShadowHostRules() const {
    return &shadow_host_rules_;
  }
  const HeapVector<RuleData>* PartPseudoRules() const {
    return &part_pseudo_rules_;
  }
  const HeapVector<RuleData>* VisitedDependentRules() const {
    return &visited_dependent_rules_;
  }
  const HeapVector<RuleData>* SelectorFragmentAnchorRules() const {
    return &selector_fragment_anchor_rules_;
  }
  const HeapVector<Member<StyleRulePage>>& PageRules() const {
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
  const HeapVector<Member<StyleRuleCounterStyle>>& CounterStyleRules() const {
    return counter_style_rules_;
  }
  const HeapVector<Member<StyleRuleFontPaletteValues>>& FontPaletteValuesRules()
      const {
    return font_palette_values_rules_;
  }
  const HeapVector<Member<StyleRuleScrollTimeline>>& ScrollTimelineRules()
      const {
    return scroll_timeline_rules_;
  }
  const HeapVector<Member<StyleRulePositionFallback>>& PositionFallbackRules()
      const {
    return position_fallback_rules_;
  }
  const HeapVector<RuleData>* SlottedPseudoElementRules() const {
    return &slotted_pseudo_element_rules_;
  }

  bool HasCascadeLayers() const { return implicit_outer_layer_; }
  const CascadeLayer& CascadeLayers() const {
    DCHECK(implicit_outer_layer_);
    return *implicit_outer_layer_;
  }

  unsigned RuleCount() const { return rule_count_; }

  void CompactRulesIfNeeded() {
    if (need_compaction_)
      CompactRules();
  }

  bool HasSlottedRules() const {
    return !slotted_pseudo_element_rules_.IsEmpty();
  }

  bool HasBucketForStyleAttribute() const { return has_bucket_for_style_attr_; }

  bool NeedsFullRecalcForRuleSetInvalidation() const {
    return features_.NeedsFullRecalcForRuleSetInvalidation();
  }

  bool DidMediaQueryResultsChange(const MediaQueryEvaluator& evaluator) const;

  // We use a vector of Interval<T> to represent that rules with positions
  // between start_position (inclusive) and the next Interval<T>'s
  // start_position (exclusive) share some property:
  //
  //   - If T = CascadeLayer, belong to the given layer.
  //   - If T = ContainerQuery, are predicated on the given container query.
  //   - If T = StyleScope, are declared in the given @style scope.
  //
  // We do this instead of putting the data directly onto the RuleData,
  // because most rules don't need these fields and websites can have a large
  // number of RuleData objects (30k+). Since neighboring rules tend to have the
  // same values for these (often nullptr), we save memory and cache space at
  // the cost of a some extra seeking through these lists when matching rules.
  template <class T>
  class Interval {
    DISALLOW_NEW();

   public:
    Interval(const T* passed_value, unsigned passed_position)
        : value(passed_value), start_position(passed_position) {}
    const Member<const T> value;
    const unsigned start_position = 0;

    void Trace(Visitor*) const;
  };

  const HeapVector<Interval<CascadeLayer>>& LayerIntervals() const {
    return layer_intervals_;
  }
  const HeapVector<Interval<ContainerQuery>>& ContainerQueryIntervals() const {
    return container_query_intervals_;
  }
  const HeapVector<Interval<StyleScope>>& ScopeIntervals() const {
    return scope_intervals_;
  }

#ifndef NDEBUG
  void Show() const;
#endif

  void Trace(Visitor*) const;

 private:
  FRIEND_TEST_ALL_PREFIXES(RuleSetTest, RuleCountNotIncreasedByInvalidRuleData);
  FRIEND_TEST_ALL_PREFIXES(RuleSetTest, RuleDataPositionLimit);
  friend class RuleSetCascadeLayerTest;

  using RuleMap = HeapHashMap<AtomicString, Member<HeapVector<RuleData>>>;
  using SubstringMatcherMap =
      HashMap<AtomicString, std::unique_ptr<base::SubstringSetMatcher>>;

  void AddToRuleSet(const AtomicString& key, RuleMap&, const RuleData&);
  void AddPageRule(StyleRulePage*);
  void AddViewportRule(StyleRuleViewport*);
  void AddFontFaceRule(StyleRuleFontFace*);
  void AddKeyframesRule(StyleRuleKeyframes*);
  void AddPropertyRule(StyleRuleProperty*);
  void AddScrollTimelineRule(StyleRuleScrollTimeline*);
  void AddCounterStyleRule(StyleRuleCounterStyle*);
  void AddFontPaletteValuesRule(StyleRuleFontPaletteValues*);
  void AddPositionFallbackRule(StyleRulePositionFallback*);

  bool MatchMediaForAddRules(const MediaQueryEvaluator& evaluator,
                             const MediaQuerySet* media_queries);
  void AddChildRules(const HeapVector<Member<StyleRuleBase>>&,
                     const MediaQueryEvaluator& medium,
                     AddRuleFlags,
                     const ContainerQuery*,
                     CascadeLayer*,
                     const StyleScope*);
  bool FindBestRuleSetAndAdd(const CSSSelector&, const RuleData&);
  void AddRule(StyleRule*,
               unsigned selector_index,
               AddRuleFlags,
               const ContainerQuery*,
               const CascadeLayer*,
               const StyleScope*);

  void SortKeyframesRulesIfNeeded();

  void CompactRules();
  static void CompactRuleMap(RuleMap&);
  static void CreateSubstringMatchers(
      RuleMap& attr_map,
      SubstringMatcherMap& substring_matcher_map);

#if DCHECK_IS_ON()
  void AssertRuleListsSorted() const;
#endif

  CascadeLayer* EnsureImplicitOuterLayer() {
    if (!implicit_outer_layer_)
      implicit_outer_layer_ = MakeGarbageCollected<CascadeLayer>();
    return implicit_outer_layer_;
  }

  CascadeLayer* GetOrAddSubLayer(CascadeLayer*,
                                 const StyleRuleBase::LayerName&);
  void AddRuleToLayerIntervals(const CascadeLayer*, unsigned position);

  // May return nullptr for the implicit outer layer.
  const CascadeLayer* GetLayerForTest(const RuleData&) const;

  RuleMap id_rules_;
  RuleMap class_rules_;
  RuleMap attr_rules_;
  // A structure for quickly rejecting an entire attribute rule set
  // (from attr_rules_). If we have many rules in the same bucket,
  // we build up a case-insensitive substring-matching structure of all
  // the values we can match on (all attribute selectors are either substring,
  // or something stricter than substring). We can then use that structure
  // to see in linear time (of the length of the attribute value in the DOM)
  // whether we can have any matches at all.
  //
  // If we find any matches, we need to recheck each rule, because the rule in
  // question may actually be case-sensitive, or we might want e.g. a prefix
  // match instead of a substring match. (We could solve prefix/suffix by
  // means of inserting special start-of-string and end-of-string tokens,
  // but we keep it simple for now.) Also, the way we use the
  // SubstringSetMatcher, we don't actually get back which rules matched.
  //
  // This element does not exist, if there are few enough rules that we don't
  // deem this step worth it, or if the build of the tree failed. (In
  // particular, if there is only a single rule in this bucket, it's
  // pointless to run the entire Aho-Corasick algorithm instead of just
  // doing a simple match.) Check GetMinimumRulesetSizeForSubstringMatcher()
  // before looking up for a cheaper test.
  SubstringMatcherMap attr_substring_matchers_;
  RuleMap tag_rules_;
  RuleMap ua_shadow_pseudo_element_rules_;
  HeapVector<RuleData> link_pseudo_class_rules_;
  HeapVector<RuleData> cue_pseudo_rules_;
  HeapVector<RuleData> focus_pseudo_class_rules_;
  HeapVector<RuleData> focus_visible_pseudo_class_rules_;
  HeapVector<RuleData> spatial_navigation_interest_class_rules_;
  HeapVector<RuleData> universal_rules_;
  HeapVector<RuleData> shadow_host_rules_;
  HeapVector<RuleData> part_pseudo_rules_;
  HeapVector<RuleData> slotted_pseudo_element_rules_;
  HeapVector<RuleData> visited_dependent_rules_;
  HeapVector<RuleData> selector_fragment_anchor_rules_;
  RuleFeatureSet features_;
  HeapVector<Member<StyleRulePage>> page_rules_;
  HeapVector<Member<StyleRuleFontFace>> font_face_rules_;
  HeapVector<Member<StyleRuleFontPaletteValues>> font_palette_values_rules_;
  HeapVector<Member<StyleRuleKeyframes>> keyframes_rules_;
  HeapVector<Member<StyleRuleProperty>> property_rules_;
  HeapVector<Member<StyleRuleCounterStyle>> counter_style_rules_;
  HeapVector<Member<StyleRuleScrollTimeline>> scroll_timeline_rules_;
  HeapVector<Member<StyleRulePositionFallback>> position_fallback_rules_;
  HeapVector<MediaQuerySetResult> media_query_set_results_;

  // Whether there is a ruleset bucket for rules with a selector on
  // the style attribute (which is rare, but allowed). If so, the caller
  // may need to take extra steps to synchronize the style attribute on
  // an element before looking for appropriate buckets.
  bool has_bucket_for_style_attr_ = false;

  unsigned rule_count_ = 0;
  bool need_compaction_ = false;

  // nullptr if the stylesheet doesn't explicitly declare any layer.
  Member<CascadeLayer> implicit_outer_layer_;
  // Empty vector if the stylesheet doesn't explicitly declare any layer.
  HeapVector<Interval<CascadeLayer>> layer_intervals_;
  // Empty vector if the stylesheet doesn't use any container queries.
  HeapVector<Interval<ContainerQuery>> container_query_intervals_;
  // Empty vector if the stylesheet doesn't use any @scopes.
  HeapVector<Interval<StyleScope>> scope_intervals_;

#ifndef NDEBUG
  HeapVector<RuleData> all_rules_;
#endif
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::RuleSet::Interval<blink::CascadeLayer>)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::RuleSet::Interval<blink::ContainerQuery>)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(
    blink::RuleSet::Interval<blink::StyleScope>)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RULE_SET_H_
