// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/invalidation/rule_invalidation_data_builder.h"

namespace blink {

RuleInvalidationDataBuilder::RuleInvalidationDataBuilder(
    RuleInvalidationData& rule_invalidation_data)
    : RuleInvalidationDataVisitor(rule_invalidation_data) {}

void RuleInvalidationDataBuilder::Merge(const RuleInvalidationData& other) {
  for (const auto& entry : other.class_invalidation_sets) {
    MergeInvalidationSet(rule_invalidation_data_.class_invalidation_sets,
                         entry.key, entry.value);
  }
  if (other.names_with_self_invalidation) {
    if (rule_invalidation_data_.names_with_self_invalidation == nullptr) {
      rule_invalidation_data_.names_with_self_invalidation =
          std::make_unique<WTF::BloomFilter<14>>();
    }
    rule_invalidation_data_.names_with_self_invalidation->Merge(
        *other.names_with_self_invalidation);
  }
  for (const auto& entry : other.attribute_invalidation_sets) {
    MergeInvalidationSet(rule_invalidation_data_.attribute_invalidation_sets,
                         entry.key, entry.value);
  }
  for (const auto& entry : other.id_invalidation_sets) {
    MergeInvalidationSet(rule_invalidation_data_.id_invalidation_sets,
                         entry.key, entry.value);
  }
  for (const auto& entry : other.pseudo_invalidation_sets) {
    auto key = static_cast<CSSSelector::PseudoType>(entry.key);
    MergeInvalidationSet(rule_invalidation_data_.pseudo_invalidation_sets, key,
                         entry.value);
  }
  if (other.universal_sibling_invalidation_set) {
    EnsureUniversalSiblingInvalidationSet()->Combine(
        *other.universal_sibling_invalidation_set);
  }
  if (other.nth_invalidation_set) {
    EnsureNthInvalidationSet()->Combine(*other.nth_invalidation_set);
  }

  for (const auto& class_name : other.classes_in_has_argument) {
    rule_invalidation_data_.classes_in_has_argument.insert(class_name);
  }
  for (const auto& attribute_name : other.attributes_in_has_argument) {
    rule_invalidation_data_.attributes_in_has_argument.insert(attribute_name);
  }
  for (const auto& id : other.ids_in_has_argument) {
    rule_invalidation_data_.ids_in_has_argument.insert(id);
  }
  for (const auto& tag_name : other.tag_names_in_has_argument) {
    rule_invalidation_data_.tag_names_in_has_argument.insert(tag_name);
  }
  rule_invalidation_data_.universal_in_has_argument |=
      other.universal_in_has_argument;
  rule_invalidation_data_.not_pseudo_in_has_argument |=
      other.not_pseudo_in_has_argument;
  for (const auto& pseudo_type : other.pseudos_in_has_argument) {
    rule_invalidation_data_.pseudos_in_has_argument.insert(pseudo_type);
  }

  rule_invalidation_data_.max_direct_adjacent_selectors =
      std::max(rule_invalidation_data_.max_direct_adjacent_selectors,
               other.max_direct_adjacent_selectors);
  rule_invalidation_data_.uses_first_line_rules |= other.uses_first_line_rules;
  rule_invalidation_data_.uses_window_inactive_selector |=
      other.uses_window_inactive_selector;
  rule_invalidation_data_.universal_in_has_argument |=
      other.universal_in_has_argument;
  rule_invalidation_data_.not_pseudo_in_has_argument |=
      other.not_pseudo_in_has_argument;
  rule_invalidation_data_.invalidates_parts |= other.invalidates_parts;
  rule_invalidation_data_.uses_has_inside_nth |= other.uses_has_inside_nth;
}

void RuleInvalidationDataBuilder::MergeInvalidationSet(
    RuleInvalidationData::InvalidationSetMap& map,
    const AtomicString& key,
    scoped_refptr<InvalidationSet> invalidation_set) {
  DCHECK(invalidation_set);
  scoped_refptr<InvalidationSet>& slot =
      map.insert(key, nullptr).stored_value->value;
  if (!slot) {
    slot = std::move(invalidation_set);
  } else {
    EnsureMutableInvalidationSet(
        invalidation_set->GetType(),
        invalidation_set->IsSelfInvalidationSet() ? kSubject : kAncestor,
        invalidation_set->InvalidatesNth(), slot)
        .Combine(*invalidation_set);
  }
}

void RuleInvalidationDataBuilder::MergeInvalidationSet(
    RuleInvalidationData::PseudoTypeInvalidationSetMap& map,
    CSSSelector::PseudoType key,
    scoped_refptr<InvalidationSet> invalidation_set) {
  DCHECK(invalidation_set);
  scoped_refptr<InvalidationSet>& slot =
      map.insert(key, nullptr).stored_value->value;
  if (!slot) {
    slot = std::move(invalidation_set);
  } else {
    EnsureMutableInvalidationSet(
        invalidation_set->GetType(),
        invalidation_set->IsSelfInvalidationSet() ? kSubject : kAncestor,
        invalidation_set->InvalidatesNth(), slot)
        .Combine(*invalidation_set);
  }
}

}  // namespace blink
