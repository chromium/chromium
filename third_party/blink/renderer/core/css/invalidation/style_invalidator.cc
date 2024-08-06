// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/invalidation/style_invalidator.h"

#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"

namespace blink {

// StyleInvalidator methods are super sensitive to performance benchmarks.
// We easily get 1% regression per additional if statement on recursive
// invalidate methods.
// To minimize performance impact, we wrap trace events with a lookup of
// cached flag. The cached flag is made "static const" and is not shared
// with InvalidationSet to avoid additional GOT lookup cost.
static const unsigned char* g_style_invalidator_tracing_enabled = nullptr;

#define TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(element, reason) \
  if (*g_style_invalidator_tracing_enabled) [[unlikely]]                 \
    TRACE_STYLE_INVALIDATOR_INVALIDATION(element, reason);

void StyleInvalidator::Invalidate(Document& document, Element* root_element) {
  SiblingData sibling_data;

  if (document.NeedsStyleInvalidation()) [[unlikely]] {
    DCHECK(root_element == document.documentElement());
    PushInvalidationSetsForContainerNode(document, sibling_data);
    document.ClearNeedsStyleInvalidation();
    DCHECK(sibling_data.IsEmpty());
  }

  if (root_element) {
    Invalidate(*root_element, sibling_data);
    if (!sibling_data.IsEmpty()) {
      for (Element* child = ElementTraversal::NextSibling(*root_element); child;
           child = ElementTraversal::NextSibling(*child)) {
        Invalidate(*child, sibling_data);
      }
    }
    for (Node* ancestor = root_element; ancestor;
         ancestor = ancestor->ParentOrShadowHostNode()) {
      ancestor->ClearChildNeedsStyleInvalidation();
    }
  }
  document.ClearChildNeedsStyleInvalidation();
  pending_invalidation_map_.clear();
  pending_nth_sets_.clear();
}

StyleInvalidator::StyleInvalidator(
    PendingInvalidationMap& pending_invalidation_map)
    : pending_invalidation_map_(pending_invalidation_map) {
  g_style_invalidator_tracing_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"));
}

StyleInvalidator::~StyleInvalidator() = default;

void StyleInvalidator::PushInvalidationSet(
    const InvalidationSet& invalidation_set) {
  DCHECK(!invalidation_flags_.WholeSubtreeInvalid());
  DCHECK(!invalidation_set.WholeSubtreeInvalid());
  DCHECK(!invalidation_set.IsEmpty());
  if (invalidation_set.CustomPseudoInvalid()) {
    invalidation_flags_.SetInvalidateCustomPseudo(true);
  }
  if (invalidation_set.TreeBoundaryCrossing()) {
    invalidation_flags_.SetTreeBoundaryCrossing(true);
  }
  if (invalidation_set.InsertionPointCrossing()) {
    invalidation_flags_.SetInsertionPointCrossing(true);
  }
  if (invalidation_set.InvalidatesSlotted()) {
    invalidation_flags_.SetInvalidatesSlotted(true);
  }
  if (invalidation_set.InvalidatesParts()) {
    invalidation_flags_.SetInvalidatesParts(true);
  }
  invalidation_sets_.push_back(&invalidation_set);
}

ALWAYS_INLINE bool StyleInvalidator::MatchesCurrentInvalidationSets(
    Element& element) const {
  if (invalidation_flags_.InvalidateCustomPseudo() &&
      element.ShadowPseudoId() != g_null_atom) {
    TRACE_STYLE_INVALIDATOR_INVALIDATION_IF_ENABLED(element,
                                                    kInvalidateCustomPseudo);
    return true;
  }

  for (auto* const invalidation_set : invalidation_sets_) {
    if (invalidation_set->InvalidatesElement(element)) {
      return true;
    }
  }

  return false;
}

bool StyleInvalidator::MatchesCurrentInvalidationSetsAsSlotted(
    Element& element) const {
  DCHECK(invalidation_flags_.InvalidatesSlotted());

  for (auto* const invalidation_set : invalidation_sets_) {
    if (!invalidation_set->InvalidatesSlotted()) {
      continue;
    }
    if (invalidation_set->InvalidatesElement(element)) {
      return true;
    }
  }
  return false;
}

bool StyleInvalidator::MatchesCurrentInvalidationSetsAsParts(
    Element& element) const {
  DCHECK(invalidation_flags_.InvalidatesParts());

  for (auto* const invalidation_set : invalidation_sets_) {
    if (!invalidation_set->InvalidatesParts()) {
      continue;
    }
    if (invalidation_set->InvalidatesElement(element)) {
      return true;
    }
  }
  return false;
}

void StyleInvalidator::SiblingData::PushInvalidationSet(
    const SiblingInvalidationSet& invalidation_set) {
  unsigned invalidation_limit;
  if (invalidation_set.MaxDirectAdjacentSelectors() == UINT_MAX) {
    invalidation_limit = UINT_MAX;
  } else {
    invalidation_limit =
        element_index_ + invalidation_set.MaxDirectAdjacentSelectors();
  }
  invalidation_entries_.push_back(Entry(&invalidation_set, invalidation_limit));
}

bool StyleInvalidator::SiblingData::MatchCurrentInvalidationSets(
    Element& element,
    StyleInvalidator& style_invalidator) {
  bool this_element_needs_style_recalc = false;
  DCHECK(!style_invalidator.WholeSubtreeInvalid());

  unsigned index = 0;
  while (index < invalidation_entries_.size()) {
    if (element_index_ > invalidation_entries_[index].invalidation_limit_) {
      // invalidation_entries_[index] only applies to earlier siblings. Remove
      // it.
      invalidation_entries_[index] = invalidation_entries_.back();
      invalidation_entries_.pop_back();
      continue;
    }

    const SiblingInvalidationSet& invalidation_set =
        *invalidation_entries_[index].invalidation_set_;
    ++index;
    if (!invalidation_set.InvalidatesElement(element)) {
      continue;
    }

    if (invalidation_set.InvalidatesSelf()) {
      this_element_needs_style_recalc = true;
    }

    if (const DescendantInvalidationSet* descendants =
            invalidation_set.SiblingDescendants()) {
      if (descendants->WholeSubtreeInvalid()) {
        TRACE_STYLE_INVALIDATOR_INVALIDATION_SET(
            element, kInvalidationSetInvalidatesSubtree, *descendants);
        element.SetNeedsStyleRecalc(
            kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                                     style_change_reason::kRelatedStyleRule));
        return true;
      }

      if (!descendants->IsEmpty()) {
        style_invalidator.PushInvalidationSet(*descendants);
      }
    }
  }
  return this_element_needs_style_recalc;
}

void StyleInvalidator::PushInvalidationSetsForContainerNode(
    ContainerNode& node,
    SiblingData& sibling_data) {
  auto pending_invalidations_iterator = pending_invalidation_map_.find(&node);
  if (pending_invalidations_iterator == pending_invalidation_map_.end()) {
    NOTREACHED_IN_MIGRATION()
        << "We should strictly not have marked an element for "
           "invalidation without any pending invalidations.";
    return;
  }
  NodeInvalidationSets& pending_invalidations =
      pending_invalidations_iterator->value;

  DCHECK(pending_nth_sets_.empty());

  for (const auto& invalidation_set : pending_invalidations.Siblings()) {
    CHECK(invalidation_set->IsAlive());
    if (invalidation_set->IsNthSiblingInvalidationSet()) {
      AddPendingNthSiblingInvalidationSet(
          To<NthSiblingInvalidationSet>(*invalidation_set));
    } else {
      sibling_data.PushInvalidationSet(
          To<SiblingInvalidationSet>(*invalidation_set));
    }
  }

  if (node.GetStyleChangeType() == kSubtreeStyleChange) {
    return;
  }

  if (!pending_invalidations.Descendants().empty()) {
    for (const auto& invalidation_set : pending_invalidations.Descendants()) {
      CHECK(invalidation_set->IsAlive());
      PushInvalidationSet(*invalidation_set);
    }
    if (*g_style_invalidator_tracing_enabled) [[unlikely]] {
      DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT_WITH_CATEGORIES(
          TRACE_DISABLED_BY_DEFAULT("devtools.timeline.invalidationTracking"),
          "StyleInvalidatorInvalidationTracking",
          inspector_style_invalidator_invalidate_event::InvalidationList, node,
          pending_invalidations.Descendants());
    }
  }
}

ALWAYS_INLINE bool StyleInvalidator::CheckInvalidationSetsAgainstElement(
    Element& element,
    SiblingData& sibling_data) {
  // We need to call both because the sibling data may invalidate the whole
  // subtree at which point we can stop recursing.
  bool matches_current = MatchesCurrentInvalidationSets(element);
  bool matches_sibling;
  if (!sibling_data.IsEmpty() &&
      sibling_data.MatchCurrentInvalidationSets(element, *this)) [[unlikely]] {
    matches_sibling = true;
  } else {
    matches_sibling = false;
  }
  return matches_current || matches_sibling;
}

void StyleInvalidator::InvalidateShadowRootChildren(Element& element) {
  if (ShadowRoot* root = element.GetShadowRoot()) {
    if (!TreeBoundaryCrossing() && !root->ChildNeedsStyleInvalidation() &&
        !root->NeedsStyleInvalidation()) {
      return;
    }
    RecursionCheckpoint checkpoint(this);
    SiblingData sibling_data;
    if (!WholeSubtreeInvalid()) {
      if (root->NeedsStyleInvalidation()) [[unlikely]] {
        // The shadow root does not have any siblings. There should never be any
        // other sets than the nth set to schedule.
        DCHECK(sibling_data.IsEmpty());
        PushInvalidationSetsForContainerNode(*root, sibling_data);
      }
    }
    PushNthSiblingInvalidationSets(sibling_data);
    for (Element* child = ElementTraversal::FirstChild(*root); child;
         child = ElementTraversal::NextSibling(*child)) {
      Invalidate(*child, sibling_data);
    }
    root->ClearChildNeedsStyleInvalidation();
    root->ClearNeedsStyleInvalidation();
  }
}

void StyleInvalidator::InvalidateChildren(Element& element) {
  if (!!element.GetShadowRoot()) [[unlikely]] {
    InvalidateShadowRootChildren(element);
  }

  // Initialization of the variable costs up to 15% on blink_perf.css
  // AttributeDescendantSelector.html benchmark.
  SiblingData sibling_data STACK_UNINITIALIZED;
  PushNthSiblingInvalidationSets(sibling_data);

  for (Element* child = ElementTraversal::FirstChild(element); child;
       child = ElementTraversal::NextSibling(*child)) {
    Invalidate(*child, sibling_data);
  }
}

void StyleInvalidator::Invalidate(Element& element, SiblingData& sibling_data) {
  sibling_data.Advance();
  // Preserves the current stack of pending invalidations and other state and
  // restores it when this method returns.
  RecursionCheckpoint checkpoint(this);

  // If we have already entered a subtree that is going to be entirely
  // recalculated then there is no need to test against current invalidation
  // sets or to continue to accumulate new invalidation sets as we descend the
  // tree.
  if (!WholeSubtreeInvalid()) {
    if (element.GetStyleChangeType() == kSubtreeStyleChange) {
      SetWholeSubtreeInvalid();
    } else if (CheckInvalidationSetsAgainstElement(element, sibling_data)) {
      element.SetNeedsStyleRecalc(kLocalStyleChange,
                                  StyleChangeReasonForTracing::Create(
                                      style_change_reason::kRelatedStyleRule));
    }
    if (element.NeedsStyleInvalidation()) [[unlikely]] {
      PushInvalidationSetsForContainerNode(element, sibling_data);
    }

    auto* html_slot_element = DynamicTo<HTMLSlotElement>(element);
    if (html_slot_element && InvalidatesSlotted()) {
      InvalidateSlotDistributedElements(*html_slot_element);
    }
  }

  // We need to recurse into children if:
  // * the whole subtree is not invalid and we have invalidation sets that
  //   could apply to the descendants.
  // * there are invalidation sets attached to descendants then we need to
  //   clear the flags on the nodes, whether we use the sets or not.
  if ((!WholeSubtreeInvalid() && HasInvalidationSets() &&
       element.GetComputedStyle()) ||
      element.ChildNeedsStyleInvalidation()) {
    InvalidateChildren(element);
  } else {
    ClearPendingNthSiblingInvalidationSets();
  }

  element.ClearChildNeedsStyleInvalidation();
  element.ClearNeedsStyleInvalidation();
}

void StyleInvalidator::InvalidateSlotDistributedElements(
    HTMLSlotElement& slot) const {
  for (auto& distributed_node : slot.FlattenedAssignedNodes()) {
    if (distributed_node->NeedsStyleRecalc()) {
      continue;
    }
    auto* element = DynamicTo<Element>(distributed_node.Get());
    if (!element) {
      continue;
    }
    if (MatchesCurrentInvalidationSetsAsSlotted(*element)) {
      distributed_node->SetNeedsStyleRecalc(
          kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                 style_change_reason::kRelatedStyleRule));
    }
  }
}

}  // namespace blink
