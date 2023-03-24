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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RULE_FEATURE_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RULE_FEATURE_SET_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_flags.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/resolver/media_query_result.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/bloom_filter.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

class ContainerNode;
class CSSSelector;
struct InvalidationLists;
class QualifiedName;
class StyleScope;

// Summarizes and indexes the contents of CSS selectors. It creates
// invalidation sets from them and makes them available via several
// CollectInvalidationSetForFoo methods which use the indices to quickly gather
// the relevant InvalidationSets for a particular DOM mutation.
//
// The name may be somewhat confusing and is for historical reasons.
// The original “features” extracted from the selectors were the ones
// in FeatureMetadata (e.g. “does any selector use ::first-line”);
// invalidation sets were added later. So even though 90% of the code
// in the class is about invalidation sets, and they are the primary
// “features” being extracted from the selector now, the file does not
// live in css/invalidation/. Perhaps this should be changed.
//
// In addition to complete invalidation (where we just throw up our
// hands and invalidate everything) and :has() (which is described in
// detail below), we have fundamentally four types of invalidation.
// All will be described for a class selector, but apply equally to
// id etc.:
//
//   - Self-invalidation: When an element gets or loses class .c,
//     that element needs to be invalidated (.c exists as a subject in
//     some selector). We represent this by a bit in .c's invalidation
//     set (or by inserting the class name in a Bloom filter; see
//     class_invalidation_sets_).
//
//   - Descendant invalidation: When an element gets or loses class .c,
//     all of its children with class .d need to be invalidated
//     (a selector of the form .c .d or .c > .d exists). We represent
//     this by storing .d in .c's descendant invalidation set.
//
//   - Sibling invalidation: When an element gets or loses class .c,
//     all of its _siblings_ with class .d need to be invalidated
//     (a selector of the form .c ~ .d or .c + .d exists).
//     We represent this by storing .d in c's sibling invalidation set.
//
//   - nth-child invalidation: Described immediately below.
//
// nth-child invalidation deals with peculiarities for :nth-child()
// and related selectors (such as :only-of-type). We have a couple
// of distinct strategies for dealing with them:
//
//   - When we add or insert a node in the DOM tree where any child
//     of the parent has matched such a selector, we forcibly schedule
//     the (global) NthSiblingInvalidationSet. In other words, this
//     is hardly related to the normal invalidation mechanism at all.
//
//   - Finally, for :nth_child(... of :has()), we get a signal when
//     a node is affected by :has() subject invalidation
//     (in StyleEngine::InvalidateElementAffectedByHas()), and can
//     forcibly schedule the NthSiblingInvalidationSet, much like
//     the previous point.
//
//   - When we have :nth-child(... of S) as a subject (ie., not just
//     pure :nth-child(), but anything with a selector), we set the
//     invalidates_nth_ bit on all invalidation sets for S. This means
//     that whenever we schedule invalidation sets for anything in S,
//     and any child of the parent has matched any :nth-child() selector,
//     we'll schedule the NthSiblingInvalidationSet.
//
//   - For all ancestors, we go through them recursively to find
//     S within :nth-child(), and set their invalidates_nth_ similarly.
//     This is conceptually the same thing as the previous point,
//     but since we already handle subjects and ancestors differently,
//     it was convenient with some mild code duplication here.
//
//   - When we have sibling selectors against :nth-child, special
//     provisions apply; see comments NthSiblingInvalidationSet.
class CORE_EXPORT RuleFeatureSet {
  DISALLOW_NEW();

 public:
  RuleFeatureSet() = default;
  RuleFeatureSet(const RuleFeatureSet&) = delete;
  RuleFeatureSet& operator=(const RuleFeatureSet&) = delete;

  bool operator==(const RuleFeatureSet&) const;
  bool operator!=(const RuleFeatureSet& o) const { return !(*this == o); }

  // Merge the given RuleFeatureSet (which remains unchanged) into this one.
  void Merge(const RuleFeatureSet&);
  void Clear();

  enum SelectorPreMatch { kSelectorNeverMatches, kSelectorMayMatch };

  // Creates invalidation sets for the given CSS selector. This is done as part
  // of creating the RuleSet for the style sheet, i.e., before matching or
  // mutation begins.
  SelectorPreMatch CollectFeaturesFromSelector(const CSSSelector&,
                                               const StyleScope*);

  // Member functions for accessing non-invalidation-set related features.
  bool UsesFirstLineRules() const { return metadata_.uses_first_line_rules; }
  bool UsesWindowInactiveSelector() const {
    return metadata_.uses_window_inactive_selector;
  }
  // Returns true if we have :nth-child(... of S) selectors where S contains a
  // :has() selector.
  bool UsesHasInsideNth() const { return metadata_.uses_has_inside_nth; }
  bool NeedsFullRecalcForRuleSetInvalidation() const {
    return metadata_.needs_full_recalc_for_rule_set_invalidation;
  }
  unsigned MaxDirectAdjacentSelectors() const {
    return metadata_.max_direct_adjacent_selectors;
  }
  bool HasSelectorForId(const AtomicString& id_value) const {
    return id_invalidation_sets_.Contains(id_value);
  }
  MediaQueryResultFlags& MutableMediaQueryResultFlags() {
    return media_query_result_flags_;
  }
  bool HasMediaQueryResults() const {
    return media_query_result_flags_.is_viewport_dependent ||
           media_query_result_flags_.is_device_dependent;
  }
  bool HasViewportDependentMediaQueries() const;
  bool HasDynamicViewportDependentMediaQueries() const;

  // Collect descendant and sibling invalidation sets, for a given type of
  // change (e.g. “if this element added or removed the given class, what other
  // types of elements need to change?”). This is called during DOM mutations.
  // CollectInvalidationSets* govern self-invalidation and descendant
  // invalidations, while CollectSiblingInvalidationSets* govern sibling
  // invalidations.

  // Note that class invalidations will sometimes return self-invalidation
  // even when it is not necessary; see comment on class_invalidation_sets_.
  void CollectInvalidationSetsForClass(InvalidationLists&,
                                       Element&,
                                       const AtomicString& class_name) const;

  void CollectInvalidationSetsForId(InvalidationLists&,
                                    Element&,
                                    const AtomicString& id) const;
  void CollectInvalidationSetsForAttribute(
      InvalidationLists&,
      Element&,
      const QualifiedName& attribute_name) const;
  void CollectInvalidationSetsForPseudoClass(InvalidationLists&,
                                             Element&,
                                             CSSSelector::PseudoType) const;

  void CollectSiblingInvalidationSetForClass(
      InvalidationLists&,
      Element&,
      const AtomicString& class_name,
      unsigned min_direct_adjacent) const;
  void CollectSiblingInvalidationSetForId(InvalidationLists&,
                                          Element&,
                                          const AtomicString& id,
                                          unsigned min_direct_adjacent) const;
  void CollectSiblingInvalidationSetForAttribute(
      InvalidationLists&,
      Element&,
      const QualifiedName& attribute_name,
      unsigned min_direct_adjacent) const;

  // TODO: Document.
  void CollectUniversalSiblingInvalidationSet(
      InvalidationLists&,
      unsigned min_direct_adjacent) const;
  void CollectNthInvalidationSet(InvalidationLists&) const;
  void CollectPartInvalidationSet(InvalidationLists&) const;
  void CollectTypeRuleInvalidationSet(InvalidationLists&, ContainerNode&) const;

  // Quick tests for whether we need to consider :has() invalidation.
  bool NeedsHasInvalidationForClass(const AtomicString& class_name) const;
  bool NeedsHasInvalidationForAttribute(
      const QualifiedName& attribute_name) const;
  bool NeedsHasInvalidationForId(const AtomicString& id) const;
  bool NeedsHasInvalidationForTagName(const AtomicString& tag_name) const;
  bool NeedsHasInvalidationForInsertedOrRemovedElement(Element&) const;
  bool NeedsHasInvalidationForPseudoClass(
      CSSSelector::PseudoType pseudo_type) const;

  inline bool NeedsHasInvalidationForClassChange() const {
    return !classes_in_has_argument_.empty();
  }
  inline bool NeedsHasInvalidationForAttributeChange() const {
    return !attributes_in_has_argument_.empty();
  }
  inline bool NeedsHasInvalidationForIdChange() const {
    return !ids_in_has_argument_.empty();
  }
  inline bool NeedsHasInvalidationForPseudoStateChange() const {
    return !pseudos_in_has_argument_.empty();
  }
  inline bool NeedsHasInvalidationForInsertionOrRemoval() const {
    return not_pseudo_in_has_argument_ || universal_in_has_argument_ ||
           !tag_names_in_has_argument_.empty() ||
           NeedsHasInvalidationForClassChange() ||
           NeedsHasInvalidationForAttributeChange() ||
           NeedsHasInvalidationForIdChange() ||
           NeedsHasInvalidationForPseudoStateChange();
  }

  bool HasIdsInSelectors() const { return id_invalidation_sets_.size() > 0; }
  bool InvalidatesParts() const { return metadata_.invalidates_parts; }

  // Format the RuleFeatureSet for debugging purposes.
  //
  //  [>] Means descendant invalidation set.
  //  [+] Means sibling invalidation set.
  //  [>+] Means sibling descendant invalidation set.
  //
  // Examples:
  //
  //      .a[>] { ... } - Descendant invalidation set class |a|.
  //      #a[+] { ... } - Sibling invalidation set for id |a|
  //  [name][>] { ... } - Descendant invalidation set for attribute |name|.
  //  :hover[>] { ... } - Descendant set for pseudo-class |hover|.
  //       *[+] { ... } - Universal sibling invalidation set.
  //    nth[+>] { ... } - Nth sibling descendant invalidation set.
  //    type[>] { ... } - Type rule invalidation set.
  //
  // META flags (omitted if false):
  //
  //  F - Uses first line rules.
  //  W - Uses window inactive selector.
  //  R - Needs full recalc for ruleset invalidation.
  //  P - Invalidates parts.
  //  ~ - Max direct siblings is kDirectAdjacentMax.
  //  <integer> - Max direct siblings is specified number (omitted if 0).
  //
  // See InvalidationSet::ToString for more information.
  String ToString() const;

 private:
  enum PositionType { kSubject, kAncestor };
  InvalidationSet* InvalidationSetForSimpleSelector(const CSSSelector&,
                                                    InvalidationType,
                                                    PositionType);

  // Inserts the given value as a key for self-invalidation.
  // Return true if the insertion was successful. (It may fail because
  // there is no Bloom filter yet.)
  bool InsertIntoSelfInvalidationBloomFilter(const AtomicString& value,
                                             int salt);
  const int kClassSalt = 13;
  const int kIdSalt = 29;

  // Each map entry is either a DescendantInvalidationSet or
  // SiblingInvalidationSet.
  // When both are needed, we store the SiblingInvalidationSet, and use it to
  // hold the DescendantInvalidationSet.
  using InvalidationSetMap =
      HashMap<AtomicString, scoped_refptr<InvalidationSet>>;
  using PseudoTypeInvalidationSetMap =
      HashMap<CSSSelector::PseudoType,
              scoped_refptr<InvalidationSet>,
              IntWithZeroKeyHashTraits<unsigned>>;
  using ValuesInHasArgument = HashSet<AtomicString>;
  using PseudosInHasArgument = HashSet<CSSSelector::PseudoType>;

  struct FeatureMetadata {
    DISALLOW_NEW();
    void Merge(const FeatureMetadata& other);
    void Clear();
    bool operator==(const FeatureMetadata&) const;
    bool operator!=(const FeatureMetadata& o) const { return !(*this == o); }

    bool uses_first_line_rules = false;
    bool uses_window_inactive_selector = false;
    bool needs_full_recalc_for_rule_set_invalidation = false;
    unsigned max_direct_adjacent_selectors = 0;
    bool invalidates_parts = false;
    // If we have a selector on the form :nth-child(... of :has(S)), any element
    // changing S will trigger that the element is "affected by subject of
    // :has", and in turn, will look up the tree for anything affected by
    // :nth-child to invalidate its siblings. However, this mechanism is
    // frequently too broad; if we have two separate rules :nth-child(an+b)
    // (without complex selector) and :has(S), such :has() invalidation will not
    // be able to distinguish between an element actually having a
    // :nth-child(... of :has(S)), and just being affected by
    // (forward-)positional rules for entirely different reasons. Thus, we will
    // over-invalidate a lot (crbug.com/1426750). Since this combination is
    // fairly rare, we make a per-stylesheet stop gap solution: We note whether
    // there are any such has-inside-nth-child rules. If not, we don't need to
    // do :nth-child() invalidation of elements affected by :has(). Of course,
    // this means that the existence of a single such rule will potentially send
    // us over the performance cliff, so we may have to revisit this solution in
    // the future.
    bool uses_has_inside_nth = false;
  };

  SelectorPreMatch CollectMetadataFromSelector(
      const CSSSelector&,
      unsigned max_direct_adjacent_selectors,
      FeatureMetadata&);

  InvalidationSet& EnsureClassInvalidationSet(const AtomicString& class_name,
                                              InvalidationType,
                                              PositionType);
  InvalidationSet& EnsureAttributeInvalidationSet(
      const AtomicString& attribute_name,
      InvalidationType,
      PositionType);
  InvalidationSet& EnsureIdInvalidationSet(const AtomicString& id,
                                           InvalidationType,
                                           PositionType);
  InvalidationSet& EnsurePseudoInvalidationSet(CSSSelector::PseudoType,
                                               InvalidationType,
                                               PositionType);
  SiblingInvalidationSet& EnsureUniversalSiblingInvalidationSet();
  NthSiblingInvalidationSet& EnsureNthInvalidationSet();
  DescendantInvalidationSet& EnsureTypeRuleInvalidationSet();
  DescendantInvalidationSet& EnsurePartInvalidationSet();

  void UpdateInvalidationSets(const CSSSelector&, const StyleScope*);

  struct InvalidationSetFeatures {
    DISALLOW_NEW();

    void Merge(const InvalidationSetFeatures& other);
    bool HasFeatures() const;
    bool HasIdClassOrAttribute() const;

    void NarrowToClass(const AtomicString& class_name) {
      if (Size() == 1 && (!ids.empty() || !classes.empty())) {
        return;
      }
      ClearFeatures();
      classes.push_back(class_name);
    }
    void NarrowToAttribute(const AtomicString& attribute) {
      if (Size() == 1 &&
          (!ids.empty() || !classes.empty() || !attributes.empty())) {
        return;
      }
      ClearFeatures();
      attributes.push_back(attribute);
    }
    void NarrowToId(const AtomicString& id) {
      if (Size() == 1 && !ids.empty()) {
        return;
      }
      ClearFeatures();
      ids.push_back(id);
    }
    void NarrowToTag(const AtomicString& tag_name) {
      if (Size() == 1) {
        return;
      }
      ClearFeatures();
      tag_names.push_back(tag_name);
    }
    void NarrowToFeatures(const InvalidationSetFeatures&);
    void ClearFeatures() {
      classes.clear();
      attributes.clear();
      ids.clear();
      tag_names.clear();
      emitted_tag_names.clear();
    }
    unsigned Size() const {
      return classes.size() + attributes.size() + ids.size() +
             tag_names.size() + emitted_tag_names.size();
    }

    Vector<AtomicString> classes;
    Vector<AtomicString> attributes;
    Vector<AtomicString> ids;
    Vector<AtomicString> tag_names;
    Vector<AtomicString> emitted_tag_names;
    unsigned max_direct_adjacent_selectors = 0;

    // descendant_features_depth is used while adding features for logical
    // combinations inside :has() pseudo class to determine whether the current
    // compound selector is in subject position or not.
    //
    // This field stores the number of child and descendant combinators
    // previously evaluated for updating features from combinator. Unlike
    // max_direct_adjacent_selectors field that indicates the max limit,
    // this field simply stores the number of child and descendant combinators.
    //
    // This field is used only for the logical combinations inside :has(), but
    // we need to count all the combinators in the entire selector so that we
    // can correctly determine whether a compound is in the subject position
    // or not.
    // (e.g. For '.a:has(:is(.b ~ .c))) .d', the descendant_features_depth for
    //  compound '.b' is not 0 but 1 since the descendant combinator was
    //  evaludated for updating features when moving from '.d' to '.a:has(...)')
    //
    // How to determine whether a compound is in subject position or not:
    // 1. If descendant_feature.descendant_features_depth > 0, then the compound
    //    is not in subject position.
    // 2. If descendant_feature.descendant_features_depth == 0,
    //   2.1. If sibling_features != nullptr, then the compound is not in
    //        subject position.
    //   2.2. Otherwise, the compound is in subject position.
    unsigned descendant_features_depth = 0;

    InvalidationFlags invalidation_flags;
    bool content_pseudo_crossing = false;
    bool has_nth_pseudo = false;
    bool has_features_for_rule_set_invalidation = false;
  };

  // Siblings which contain nested selectors (e.g. :is) only count as one
  // sibling on the level where the nesting pseudo appears. To calculate
  // the max direct adjacent count correctly for each level, we sometimes
  // need to reset the count at certain boundaries.
  //
  // Example: .a + :is(.b + .c, .d + .e) + .f
  //
  // When processing the above selector, the InvalidationSetFeatures produced
  // from '.f' is eventually passed to both '.b + .c' and '.d + .e' as a mutable
  // reference. Each of those selectors will then increment the max direct
  // adjacent counter, and without a timely reset, changes would leak from one
  // sub-selector to another. It would also leak out of the :is() pseudo,
  // resulting in the wrong count for '.a' as well.
  class AutoRestoreMaxDirectAdjacentSelectors {
    STACK_ALLOCATED();

   public:
    explicit AutoRestoreMaxDirectAdjacentSelectors(
        InvalidationSetFeatures* features)
        : features_(features),
          original_value_(features ? features->max_direct_adjacent_selectors
                                   : 0) {}
    ~AutoRestoreMaxDirectAdjacentSelectors() {
      if (features_) {
        features_->max_direct_adjacent_selectors = original_value_;
      }
    }

   private:
    InvalidationSetFeatures* features_;
    unsigned original_value_ = 0;
  };

  // While adding features to the invalidation sets for the complex selectors
  // in :is() inside :has(), we need to differentiate whether the :has() is in
  // subject position or not if there is no sibling_features.
  //
  // - case 1) .a:has(:is(.b ~ .c))     : Add features as if we have .b ~ .a
  // - case 2) .a:has(:is(.b ~ .c)) .d  : add features as if we have .b ~ .a .d
  //
  // For .b in case 1, we need to use descendant_features as sibling_features.
  // But for .b in case 2, we need to extract sibling features from the compound
  // selector containing the :has() pseudo class.
  //
  // By maintaining a descendant depth information to descendant_features
  // object, we can determine whether the current compound is in subject
  // position or not. The descendant features depth will be increased when
  // RuleFeatureSet meets descendant or child combinator while adding features.
  //
  // Example)
  // - .a:has(:is(.b ~ .c))         : At .b, the descendant_features_depth is 0
  // - .a:has(:is(.b ~ .c)) .d      : At .b, the descendant_features_depth is 1
  // - .a:has(:is(.b ~ .c)) .d ~ .e : At .b, the descendant_features_depth is 1
  // - .a:has(:is(.b ~ .c)) .d > .e : At .b, the descendant_features_depth is 2
  //
  // To keep the correct depth in the descendant_features object for each level
  // of nested logical combinations, this class is used.
  class AutoRestoreDescendantFeaturesDepth {
    STACK_ALLOCATED();

   public:
    explicit AutoRestoreDescendantFeaturesDepth(
        InvalidationSetFeatures* features)
        : features_(features),
          original_value_(features ? features->descendant_features_depth : 0) {}
    ~AutoRestoreDescendantFeaturesDepth() {
      if (features_) {
        features_->descendant_features_depth = original_value_;
      }
    }

   private:
    InvalidationSetFeatures* features_;
    unsigned original_value_ = 0;
  };

  // For .a :has(:is(.b .c)).d, the invalidation set for .b is marked as whole-
  // subtree-invalid because :has() is in subject position and evaluated before
  // .b. But the invalidation set for .a can have descendant class .d. In this
  // case, the descendant_features for the same compound selector can have two
  // different state of WholeSubtreeInvalid flag. To keep the correct flag,
  // this class is used.
  class AutoRestoreWholeSubtreeInvalid {
    STACK_ALLOCATED();

   public:
    explicit AutoRestoreWholeSubtreeInvalid(InvalidationSetFeatures& features)
        : features_(features),
          original_value_(features.invalidation_flags.WholeSubtreeInvalid()) {}
    ~AutoRestoreWholeSubtreeInvalid() {
      features_.invalidation_flags.SetWholeSubtreeInvalid(original_value_);
    }

   private:
    InvalidationSetFeatures& features_;
    bool original_value_;
  };

  // For :is(:host(.a), .b) .c, the invalidation set for .a should be marked
  // as tree-crossing, but the invalidation set for .b should not.
  class AutoRestoreTreeBoundaryCrossingFlag {
    STACK_ALLOCATED();

   public:
    explicit AutoRestoreTreeBoundaryCrossingFlag(
        InvalidationSetFeatures& features)
        : features_(features),
          original_value_(features.invalidation_flags.TreeBoundaryCrossing()) {}
    ~AutoRestoreTreeBoundaryCrossingFlag() {
      features_.invalidation_flags.SetTreeBoundaryCrossing(original_value_);
    }

   private:
    InvalidationSetFeatures& features_;
    bool original_value_;
  };

  // For :is(.a, :host-context(.b), .c) .d, the invalidation set for .c should
  // not be marked as insertion point crossing.
  class AutoRestoreInsertionPointCrossingFlag {
    STACK_ALLOCATED();

   public:
    explicit AutoRestoreInsertionPointCrossingFlag(
        InvalidationSetFeatures& features)
        : features_(features),
          original_value_(
              features.invalidation_flags.InsertionPointCrossing()) {}
    ~AutoRestoreInsertionPointCrossingFlag() {
      features_.invalidation_flags.SetInsertionPointCrossing(original_value_);
    }

   private:
    InvalidationSetFeatures& features_;
    bool original_value_;
  };

  static void ExtractInvalidationSetFeature(const CSSSelector&,
                                            InvalidationSetFeatures&);

  enum FeatureInvalidationType {
    kNormalInvalidation,
    kRequiresSubtreeInvalidation
  };

  // Extracts features for the given complex selector, and adds those features
  // the appropriate invalidation sets.
  //
  // The returned InvalidationSetFeatures contain the descendant features,
  // extracted from the rightmost compound selector.
  //
  // The PositionType indicates whether or not the complex selector resides
  // in the rightmost compound (kSubject), or anything to the left of that
  // (kAncestor). For example, for ':is(.a .b) :is(.c .d)', the nested
  // complex selector '.c .d' should be called with kSubject, and the '.a .b'
  // should be called with kAncestor.
  //
  // The PseudoType indicates whether or not we are inside a nested complex
  // selector. For example, for :is(.a .b), this function is called with
  // CSSSelector equal to '.a .b', and PseudoType equal to kPseudoIs.
  // For top-level complex selectors, the PseudoType is kPseudoUnknown.
  FeatureInvalidationType UpdateInvalidationSetsForComplex(
      const CSSSelector&,
      bool in_nth_child,
      const StyleScope*,
      InvalidationSetFeatures&,
      PositionType,
      CSSSelector::PseudoType);

  void ExtractInvalidationSetFeaturesFromSimpleSelector(
      const CSSSelector&,
      InvalidationSetFeatures&);
  const CSSSelector* ExtractInvalidationSetFeaturesFromCompound(
      const CSSSelector&,
      InvalidationSetFeatures&,
      PositionType,
      bool for_logical_combination_in_has,
      bool in_nth_child);
  void ExtractInvalidationSetFeaturesFromSelectorList(const CSSSelector&,
                                                      bool in_nth_child,
                                                      InvalidationSetFeatures&,
                                                      PositionType);
  void UpdateFeaturesFromCombinator(
      CSSSelector::RelationType,
      const CSSSelector* last_compound_selector_in_adjacent_chain,
      InvalidationSetFeatures& last_compound_in_adjacent_chain_features,
      InvalidationSetFeatures*& sibling_features,
      InvalidationSetFeatures& descendant_features,
      bool for_logical_combination_in_has,
      bool in_nth_child);
  void UpdateFeaturesFromStyleScope(
      const StyleScope&,
      InvalidationSetFeatures& descendant_features);

  void AddFeaturesToInvalidationSet(InvalidationSet&,
                                    const InvalidationSetFeatures&);
  void AddFeaturesToInvalidationSets(
      const CSSSelector&,
      bool in_nth_child,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features);
  const CSSSelector* AddFeaturesToInvalidationSetsForCompoundSelector(
      const CSSSelector&,
      bool in_nth_child,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features);
  void AddFeaturesToInvalidationSetsForSimpleSelector(
      const CSSSelector& simple_selector,
      const CSSSelector& compound,
      bool in_nth_child,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features);
  void AddFeaturesToInvalidationSetsForSelectorList(
      const CSSSelector&,
      bool in_nth_child,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features);
  void AddFeaturesToInvalidationSetsForStyleScope(
      const StyleScope&,
      InvalidationSetFeatures& descendant_features);
  void AddFeaturesToUniversalSiblingInvalidationSet(
      const InvalidationSetFeatures& sibling_features,
      const InvalidationSetFeatures& descendant_features);
  void AddValuesInComplexSelectorInsideIsWhereNot(
      const CSSSelector* selector_first);
  bool AddValueOfSimpleSelectorInHasArgument(
      const CSSSelector& has_pseudo_class);

  void UpdateRuleSetInvalidation(const InvalidationSetFeatures&);
  void CollectValuesInHasArgument(const CSSSelector& has_pseudo_class);

  // The logical combinations like ':is()', ':where()' and ':not()' can cause
  // a compound selector in ':has()' to match an element outside of the ':has()'
  // argument checking scope. (:has() anchor element, its ancestors, its
  // previous siblings or its ancestor previous siblings)
  // To support invalidation for a mutation on the elements, we can add features
  // in invalidation sets only for the complex selectors in :is() inside :has()
  // as if we have another rule with simple selector.
  //
  // Example 1) '.a:has(:is(.b .c)) {}'
  //   - For class 'b' change, invalidate descendant '.a' ('.b .a {}')
  //
  // Example 2) '.a:has(~ :is(.b ~ .c)) {}'
  //   - For class 'b' change, invalidate sibling '.a' ('.b ~ .a {}')
  //
  // Example 3) '.a:has(~ :is(.b ~ .c)) .d {}'
  //   - For class 'b' change, invalidate descendant '.d' of sibling '.a'.
  //     ('.b ~ .a .d {}')
  //
  // Example 4) '.a:has(:is(.b ~ .c .d)) {}'
  //   - For class 'b' change, invalidate descendant '.a' of sibling '.c'
  //     ('.b ~ .c .a {}'), and invalidate sibling '.a' ('.b ~ .a {}').
  void AddFeaturesToInvalidationSetsForHasPseudoClass(
      const CSSSelector& has_pseudo_class,
      const CSSSelector* compound_containing_has,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features,
      bool in_nth_child);

  // There are two methods to add features for logical combinations in :has().
  // - kForAllNonRightmostCompounds:
  //     Add features as if the non-subject part of the logical combination
  //     argument is prepended to the compound containing :has().
  //     (e.g. In the above example, Example 1, 2, 3 and '.b ~ .c .a' of
  //      Example 4)
  // - kForCompoundImmediatelyFollowsAdjacentRelation:
  //     Add features as if an adjacent combinator and its next compound
  //     selector are prepended to the compound containing :has().
  //     (e.g. In the above example, '.b ~ .a' of Example 4)
  //
  // Due to the difference between the two methods (how the features are
  // updated from combinators), sibling features or descendant features for
  // a certain compound can be different per the method.
  // - For '.a:has(:is(.b ~ .c .d)) ~ .e',
  //   - At '.b' when kForAllNonRightmostCompounds:
  //     - sibling_features == '.c' / descendant_features == '.e'
  //   - At '.b' when kForCompoundImmediatelyFollowsAdjacentRelation:
  //     - sibling_features == descendant_features == '.e'
  //
  // To avoid maintaining multiple 'sibling_features' and 'descendant_features'
  // for each compound selector, features are added separately for each method.
  // (Call AddFeaturesToInvalidationSetsForLogicalCombinationInHas() for each
  //  method in AddFeaturesToInvalidationSetsForHasPseudoClass())
  enum AddFeaturesMethodForLogicalCombinationInHas {
    kForAllNonRightmostCompounds,
    kForCompoundImmediatelyFollowsAdjacentRelation
  };

  // AddFeaturesToInvalidationSetsForLogicalCombinationInHas() is invoked for
  // each logical combination inside :has(). Same as the usual feature adding
  // logic, sibling features and descendant features extracted from the
  // previous compounds are passed though 'sibling_features' and
  // 'descendant_features' arguments.
  //
  // The rightmost compound of a non-nested logical combinations is always
  // in the :has() argument checking scope.
  // - '.c' in '.a:has(:is(.b .c) .d)' is always a descendant of :has() anchor
  //   element.
  //
  // But the rightmost compound of a nested logical combinations can be or
  // cannot be in the :has() argument checking scope.
  // - '.c' in '.a:has(:is(:is(.b .c) .d))' can be a :has() anchor element or
  //   its ancestor.
  // - '.d' in '.a:has(:is(.b :is(.c .d)))' is always a descendant of :has()
  //   anchor element.
  //
  // To differentiate between the two cases, this method has an argument
  // 'previous_combinator' that represents the previous combinator evaluated
  // for updating features for logical combination inside :has().
  // The argument is always kSubSelector when the method is called for the
  // non-nested logical combinations inside :has() (when the method is called
  // in AddFeaturesToInvalidationSetsForHasPseudoClass()).
  // For the rest compounds, after the rightmost compound is skipped, the value
  // is changed to the combinator at the left of the compound.
  void AddFeaturesToInvalidationSetsForLogicalCombinationInHas(
      const CSSSelector& logical_combination,
      const CSSSelector* compound_containing_has,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features,
      CSSSelector::RelationType previous_combinator,
      AddFeaturesMethodForLogicalCombinationInHas);

  void UpdateFeaturesFromCombinatorForLogicalCombinationInHas(
      CSSSelector::RelationType combinator,
      const CSSSelector* last_compound_selector_in_adjacent_chain,
      InvalidationSetFeatures& last_compound_in_adjacent_chain_features,
      InvalidationSetFeatures*& sibling_features,
      InvalidationSetFeatures& descendant_features);
  const CSSSelector* SkipAddingAndGetLastInCompoundForLogicalCombinationInHas(
      const CSSSelector* compound_in_logical_combination,
      const CSSSelector* compound_containing_has,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features,
      CSSSelector::RelationType previous_combinator,
      AddFeaturesMethodForLogicalCombinationInHas);
  const CSSSelector* AddFeaturesAndGetLastInCompoundForLogicalCombinationInHas(
      const CSSSelector* compound_in_logical_combination,
      const CSSSelector* compound_containing_has,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features,
      CSSSelector::RelationType previous_combinator,
      AddFeaturesMethodForLogicalCombinationInHas);

  // Go recursively through everything in the given selector
  // (which is typically an ancestor; see the class comment)
  // and mark the invalidation sets of any simple selectors within it
  // for Nth-child invalidation.
  void MarkInvalidationSetsWithinNthChild(const CSSSelector& selector,
                                          bool in_nth_child);

  // Make sure that the pointer in “invalidation_set” has a single
  // reference that can be modified safely. (This is done through
  // copy-on-write, if needed, so that it can be modified without
  // disturbing unrelated invalidation sets that shared the pointer.)
  // If invalidation_set is nullptr, a new one is created. If an existing
  // InvalidationSet is used as base, it is extended to the right type
  // (descendant, sibling, self -- n-th sibling is treated as sibling)
  // if needed.
  //
  // The return value is the invalidation set to be modified. This is
  // identical to the new value of invalidation_set in all cases _except_
  // if the existing invalidation was a sibling invalidation set and
  // you requested a descendant invalidation set -- if so, it is a reference
  // to the DescendantInvalidationSet embedded within that set.
  // In other words, you must ignore the value of invalidation_set
  // after this function, since it is not what you requested.
  static InvalidationSet& EnsureMutableInvalidationSet(
      InvalidationType type,
      PositionType position,
      scoped_refptr<InvalidationSet>& invalidation_set);

  static InvalidationSet& EnsureInvalidationSet(InvalidationSetMap&,
                                                const AtomicString& key,
                                                InvalidationType,
                                                PositionType);
  static InvalidationSet& EnsureInvalidationSet(PseudoTypeInvalidationSetMap&,
                                                CSSSelector::PseudoType key,
                                                InvalidationType,
                                                PositionType);

  // Adds an InvalidationSet to this RuleFeatureSet, combining with any
  // data that may already be there. (That data may come from a previous
  // call to EnsureInvalidationSet(), or from another MergeInvalidationSet().)
  //
  // Copy-on-write is used to get correct merging in face of shared
  // InvalidationSets between keys; see comments on
  // EnsureMutableInvalidationSet() for more details.
  void MergeInvalidationSet(InvalidationSetMap&,
                            const AtomicString& key,
                            scoped_refptr<InvalidationSet>);
  void MergeInvalidationSet(PseudoTypeInvalidationSetMap&,
                            CSSSelector::PseudoType key,
                            scoped_refptr<InvalidationSet>);

  FeatureMetadata metadata_;

  // Class and ID invalidation have a special rule that is different from the
  // other sets; we do not store self-invalidation entries directly, but as a
  // Bloom filter (which can have false positives) keyed on the class/ID name's
  // AtomicString hash (multiplied with kClassSalt or kIdSalt).
  //
  // The reason is that some pages have huge amounts of simple rules of the type
  // “.foo { ...rules... }”, which would cause one such entry (consisting of the
  // self-invalidation bit only) per class rule. Dropping them and making them
  // implicit saves a lot of memory for such sites; the downside is that we can
  // get false positives. (For our 2 kB Bloom filter with two hash functions
  // and 16384 slots, we can store about 2000 such classes with a 95% rejection
  // rate. For 10000 classes, the rejection rate drops to 50%.)
  //
  // In particular, if you have an element and set class="bar" and there is no
  // rule for .bar, you may still get self-invalidation for the element. Worse,
  // when inserting a new style sheet or inserting/deleting rules, _any_ element
  // with class="" can get self-invalidated unless the Bloom filter stops it
  // (which depends strongly on how many such classes there are). So this is a
  // tradeoff. We could perhaps be more intelligent about not inserting into the
  // Bloom filter if we had to insert sibling or descendant sets too, but this
  // seems a bit narrow in practice.
  InvalidationSetMap class_invalidation_sets_;
  std::unique_ptr<WTF::BloomFilter<14>> names_with_self_invalidation_;

  // We don't create the Bloom filter right away; there may be so few of
  // them that we don't really bother. This number counts the times we've
  // inserted something that could go in there; once it reaches 50
  // (for this style sheet), we create the Bloom filter and start
  // inserting there instead. Note that we don't _remove_ any of the sets,
  // though; they will remain. This also means that when merging the
  // RuleFeatureSets into the global one, we can go over 50 such entries
  // in total.
  unsigned num_candidates_for_names_bloom_filter_ = 0;

  InvalidationSetMap attribute_invalidation_sets_;
  InvalidationSetMap
      id_invalidation_sets_;  // See comment on class_invalidation_sets_.
  PseudoTypeInvalidationSetMap pseudo_invalidation_sets_;
  scoped_refptr<SiblingInvalidationSet> universal_sibling_invalidation_set_;
  scoped_refptr<NthSiblingInvalidationSet> nth_invalidation_set_;
  scoped_refptr<DescendantInvalidationSet> type_rule_invalidation_set_;
  MediaQueryResultFlags media_query_result_flags_;
  ValuesInHasArgument classes_in_has_argument_;
  ValuesInHasArgument attributes_in_has_argument_;
  ValuesInHasArgument ids_in_has_argument_;
  ValuesInHasArgument tag_names_in_has_argument_;
  bool universal_in_has_argument_{false};
  // We always need to invalidate on insertion/removal when we have :not()
  // inside :has().
  bool not_pseudo_in_has_argument_{false};
  PseudosInHasArgument pseudos_in_has_argument_;

  friend class RuleFeatureSetTest;
  friend struct AddFeaturesToInvalidationSetsForLogicalCombinationInHasContext;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const RuleFeatureSet&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RULE_FEATURE_SET_H_
