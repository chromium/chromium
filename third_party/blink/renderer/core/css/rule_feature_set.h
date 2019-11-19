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

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_selector.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_flags.h"
#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/media_query_evaluator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class ContainerNode;
struct InvalidationLists;
class QualifiedName;
class RuleData;

// Summarizes and indexes the contents of RuleData objects. It creates
// invalidation sets from rule data and makes them available via several
// CollectInvalidationSetForFoo methods which use the indices to quickly gather
// the relevant InvalidationSets for a particular DOM mutation.
class CORE_EXPORT RuleFeatureSet {
  DISALLOW_NEW();

 public:
  RuleFeatureSet();
  ~RuleFeatureSet();

  // Methods for updating the data in this object.
  void Add(const RuleFeatureSet&);
  void Clear();

  enum SelectorPreMatch { kSelectorNeverMatches, kSelectorMayMatch };

  SelectorPreMatch CollectFeaturesFromRuleData(const RuleData*);

  // Methods for accessing the data in this object.
  bool UsesFirstLineRules() const { return metadata_.uses_first_line_rules; }
  bool UsesWindowInactiveSelector() const {
    return metadata_.uses_window_inactive_selector;
  }
  bool NeedsFullRecalcForRuleSetInvalidation() const {
    return metadata_.needs_full_recalc_for_rule_set_invalidation;
  }

  unsigned MaxDirectAdjacentSelectors() const {
    return metadata_.max_direct_adjacent_selectors;
  }

  bool HasSelectorForAttribute(const AtomicString& attribute_name) const {
    DCHECK(!attribute_name.IsEmpty());
    return attribute_invalidation_sets_.Contains(attribute_name);
  }

  bool HasSelectorForClass(const AtomicString& class_value) const {
    DCHECK(!class_value.IsEmpty());
    return class_invalidation_sets_.Contains(class_value);
  }

  bool HasSelectorForId(const AtomicString& id_value) const {
    return id_invalidation_sets_.Contains(id_value);
  }

  const MediaQueryResultList& ViewportDependentMediaQueryResults() const {
    return viewport_dependent_media_query_results_;
  }
  const MediaQueryResultList& DeviceDependentMediaQueryResults() const {
    return device_dependent_media_query_results_;
  }
  MediaQueryResultList& ViewportDependentMediaQueryResults() {
    return viewport_dependent_media_query_results_;
  }
  MediaQueryResultList& DeviceDependentMediaQueryResults() {
    return device_dependent_media_query_results_;
  }

  // Collect descendant and sibling invalidation sets.
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
  void CollectUniversalSiblingInvalidationSet(
      InvalidationLists&,
      unsigned min_direct_adjacent) const;
  void CollectNthInvalidationSet(InvalidationLists&) const;
  void CollectPartInvalidationSet(InvalidationLists&) const;
  void CollectTypeRuleInvalidationSet(InvalidationLists&, ContainerNode&) const;

  bool HasIdsInSelectors() const { return id_invalidation_sets_.size() > 0; }
  bool InvalidatesParts() const { return metadata_.invalidates_parts; }

  bool IsAlive() const { return is_alive_; }

 protected:
  enum PositionType { kSubject, kAncestor };
  InvalidationSet* InvalidationSetForSimpleSelector(const CSSSelector&,
                                                    InvalidationType,
                                                    PositionType);

 private:
  // Each map entry is either a DescendantInvalidationSet or
  // SiblingInvalidationSet.
  // When both are needed, we store the SiblingInvalidationSet, and use it to
  // hold the DescendantInvalidationSet.
  using InvalidationSetMap =
      HashMap<AtomicString, scoped_refptr<InvalidationSet>>;
  using PseudoTypeInvalidationSetMap =
      HashMap<CSSSelector::PseudoType,
              scoped_refptr<InvalidationSet>,
              WTF::IntHash<unsigned>,
              WTF::UnsignedWithZeroKeyHashTraits<unsigned>>;

  struct FeatureMetadata {
    DISALLOW_NEW();
    void Add(const FeatureMetadata& other);
    void Clear();

    bool uses_first_line_rules = false;
    bool uses_window_inactive_selector = false;
    bool needs_full_recalc_for_rule_set_invalidation = false;
    unsigned max_direct_adjacent_selectors = 0;
    bool invalidates_parts = false;
  };

  SelectorPreMatch CollectFeaturesFromSelector(const CSSSelector&,
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

  void UpdateInvalidationSets(const RuleData*);

  struct InvalidationSetFeatures {
    DISALLOW_NEW();

    void Add(const InvalidationSetFeatures& other);
    bool HasFeatures() const;
    bool HasIdClassOrAttribute() const;

    void NarrowToClass(const AtomicString& class_name) {
      if (Size() == 1 && (!ids.IsEmpty() || !classes.IsEmpty()))
        return;
      ClearFeatures();
      classes.push_back(class_name);
    }
    void NarrowToAttribute(const AtomicString& attribute) {
      if (Size() == 1 &&
          (!ids.IsEmpty() || !classes.IsEmpty() || !attributes.IsEmpty()))
        return;
      ClearFeatures();
      attributes.push_back(attribute);
    }
    void NarrowToId(const AtomicString& id) {
      if (Size() == 1 && !ids.IsEmpty())
        return;
      ClearFeatures();
      ids.push_back(id);
    }
    void NarrowToTag(const AtomicString& tag_name) {
      if (Size() == 1)
        return;
      ClearFeatures();
      tag_names.push_back(tag_name);
    }
    void NarrowToFeatures(const InvalidationSetFeatures&);
    void ClearFeatures() {
      classes.clear();
      attributes.clear();
      ids.clear();
      tag_names.clear();
    }
    unsigned Size() const {
      return classes.size() + attributes.size() + ids.size() + tag_names.size();
    }

    Vector<AtomicString> classes;
    Vector<AtomicString> attributes;
    Vector<AtomicString> ids;
    Vector<AtomicString> tag_names;
    unsigned max_direct_adjacent_selectors = 0;
    InvalidationFlags invalidation_flags;
    bool content_pseudo_crossing = false;
    bool has_nth_pseudo = false;
    bool has_features_for_rule_set_invalidation = false;
  };

  static void ExtractInvalidationSetFeature(const CSSSelector&,
                                            InvalidationSetFeatures&);

  enum FeatureInvalidationType {
    kNormalInvalidation,
    kRequiresSubtreeInvalidation
  };

  void ExtractInvalidationSetFeaturesFromSimpleSelector(
      const CSSSelector&,
      InvalidationSetFeatures&);
  const CSSSelector* ExtractInvalidationSetFeaturesFromCompound(
      const CSSSelector&,
      InvalidationSetFeatures&,
      PositionType,
      CSSSelector::PseudoType = CSSSelector::kPseudoUnknown);
  FeatureInvalidationType ExtractInvalidationSetFeaturesFromSelectorList(
      const CSSSelector&,
      InvalidationSetFeatures&,
      PositionType);
  void UpdateFeaturesFromCombinator(
      const CSSSelector&,
      const CSSSelector* last_compound_selector_in_adjacent_chain,
      InvalidationSetFeatures& last_compound_in_adjacent_chain_features,
      InvalidationSetFeatures*& sibling_features,
      InvalidationSetFeatures& descendant_features);

  void AddFeaturesToInvalidationSet(InvalidationSet&,
                                    const InvalidationSetFeatures&);
  void AddFeaturesToInvalidationSets(
      const CSSSelector&,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features);
  const CSSSelector* AddFeaturesToInvalidationSetsForCompoundSelector(
      const CSSSelector&,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features);
  void AddFeaturesToInvalidationSetsForSimpleSelector(
      const CSSSelector&,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features);
  void AddFeaturesToInvalidationSetsForSelectorList(
      const CSSSelector&,
      InvalidationSetFeatures* sibling_features,
      InvalidationSetFeatures& descendant_features);
  void AddFeaturesToUniversalSiblingInvalidationSet(
      const InvalidationSetFeatures& sibling_features,
      const InvalidationSetFeatures& descendant_features);

  void UpdateRuleSetInvalidation(const InvalidationSetFeatures&);

  static InvalidationSet& EnsureMutableInvalidationSet(
      scoped_refptr<InvalidationSet>&,
      InvalidationType,
      PositionType);

  InvalidationSet& EnsureInvalidationSet(InvalidationSetMap&,
                                         const AtomicString& key,
                                         InvalidationType,
                                         PositionType);
  InvalidationSet& EnsureInvalidationSet(PseudoTypeInvalidationSetMap&,
                                         CSSSelector::PseudoType key,
                                         InvalidationType,
                                         PositionType);

  // Adds an InvalidationSet to this RuleFeatureSet.
  //
  // A copy-on-write mechanism is used: if we don't already have an invalidation
  // set for |key|, we simply retain the incoming invalidation set without
  // copying any data. If another AddInvalidationSet call takes place with the
  // same key, we copy the existing InvalidationSet (if necessary) before
  // combining it with the incoming InvalidationSet.
  void AddInvalidationSet(InvalidationSetMap&,
                          const AtomicString& key,
                          scoped_refptr<InvalidationSet>);
  void AddInvalidationSet(PseudoTypeInvalidationSetMap&,
                          CSSSelector::PseudoType key,
                          scoped_refptr<InvalidationSet>);

  FeatureMetadata metadata_;
  InvalidationSetMap class_invalidation_sets_;
  InvalidationSetMap attribute_invalidation_sets_;
  InvalidationSetMap id_invalidation_sets_;
  PseudoTypeInvalidationSetMap pseudo_invalidation_sets_;
  scoped_refptr<SiblingInvalidationSet> universal_sibling_invalidation_set_;
  scoped_refptr<NthSiblingInvalidationSet> nth_invalidation_set_;
  scoped_refptr<DescendantInvalidationSet> type_rule_invalidation_set_;
  MediaQueryResultList viewport_dependent_media_query_results_;
  MediaQueryResultList device_dependent_media_query_results_;

  // If true, the RuleFeatureSet is alive and can be used.
  unsigned is_alive_ : 1;

  friend class RuleFeatureSetTest;
  DISALLOW_COPY_AND_ASSIGN(RuleFeatureSet);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RULE_FEATURE_SET_H_
