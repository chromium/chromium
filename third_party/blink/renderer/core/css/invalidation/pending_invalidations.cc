// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/invalidation/pending_invalidations.h"

#include "third_party/blink/renderer/core/css/invalidation/invalidation_set.h"
#include "third_party/blink/renderer/core/css/invalidation/style_invalidator.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

PendingInvalidations::PendingInvalidations() {
  InvalidationSet::CacheTracingFlag();
}

void PendingInvalidations::ScheduleInvalidationSetsForNode(
    const InvalidationLists& invalidation_lists,
    ContainerNode& node) {
  DCHECK(node.InActiveDocument());
  bool requires_descendant_invalidation = false;

  if (node.GetStyleChangeType() < kSubtreeStyleChange) {
    for (auto& invalidation_set : invalidation_lists.descendants) {
      if (invalidation_set->WholeSubtreeInvalid()) {
        node.SetNeedsStyleRecalc(kSubtreeStyleChange,
                                 StyleChangeReasonForTracing::Create(
                                     style_change_reason::kStyleInvalidator));
        requires_descendant_invalidation = false;
        break;
      }

      if (invalidation_set->InvalidatesSelf()) {
        node.SetNeedsStyleRecalc(kLocalStyleChange,
                                 StyleChangeReasonForTracing::Create(
                                     style_change_reason::kStyleInvalidator));
      }

      if (!invalidation_set->IsEmpty())
        requires_descendant_invalidation = true;
    }
  }

  if (!requires_descendant_invalidation &&
      (invalidation_lists.siblings.IsEmpty() || !node.nextSibling()))
    return;

  node.SetNeedsStyleInvalidation();

  NodeInvalidationSets& pending_invalidations =
      EnsurePendingInvalidations(node);
  if (node.nextSibling()) {
    for (auto& invalidation_set : invalidation_lists.siblings) {
      if (pending_invalidations.Siblings().Contains(invalidation_set))
        continue;
      pending_invalidations.Siblings().push_back(invalidation_set);
    }
  }

  if (!requires_descendant_invalidation)
    return;

  for (auto& invalidation_set : invalidation_lists.descendants) {
    DCHECK(!invalidation_set->WholeSubtreeInvalid());
    if (invalidation_set->IsEmpty())
      continue;
    if (pending_invalidations.Descendants().Contains(invalidation_set))
      continue;
    pending_invalidations.Descendants().push_back(invalidation_set);
  }
}

void PendingInvalidations::ScheduleSiblingInvalidationsAsDescendants(
    const InvalidationLists& invalidation_lists,
    ContainerNode& scheduling_parent) {
  DCHECK(invalidation_lists.descendants.IsEmpty());

  if (invalidation_lists.siblings.IsEmpty())
    return;

  NodeInvalidationSets& pending_invalidations =
      EnsurePendingInvalidations(scheduling_parent);

  scheduling_parent.SetNeedsStyleInvalidation();

  for (auto& invalidation_set : invalidation_lists.siblings) {
    if (invalidation_set->WholeSubtreeInvalid()) {
      scheduling_parent.SetNeedsStyleRecalc(
          kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                                   style_change_reason::kStyleInvalidator));
      return;
    }
    if (invalidation_set->InvalidatesSelf() &&
        !pending_invalidations.Descendants().Contains(invalidation_set))
      pending_invalidations.Descendants().push_back(invalidation_set);

    if (DescendantInvalidationSet* descendants =
            ToSiblingInvalidationSet(*invalidation_set).SiblingDescendants()) {
      if (descendants->WholeSubtreeInvalid()) {
        scheduling_parent.SetNeedsStyleRecalc(
            kSubtreeStyleChange, StyleChangeReasonForTracing::Create(
                                     style_change_reason::kStyleInvalidator));
        return;
      }
      if (!pending_invalidations.Descendants().Contains(descendants))
        pending_invalidations.Descendants().push_back(descendants);
    }
  }
}

void PendingInvalidations::RescheduleSiblingInvalidationsAsDescendants(
    Element& element) {
  DCHECK(element.parentNode());
  auto pending_invalidations_iterator =
      pending_invalidation_map_.find(&element);
  if (pending_invalidations_iterator == pending_invalidation_map_.end() ||
      pending_invalidations_iterator->value.Siblings().IsEmpty())
    return;
  NodeInvalidationSets& pending_invalidations =
      pending_invalidations_iterator->value;

  InvalidationLists invalidation_lists;
  for (const auto& invalidation_set : pending_invalidations.Siblings()) {
    invalidation_lists.descendants.push_back(invalidation_set);
    if (DescendantInvalidationSet* descendants =
            ToSiblingInvalidationSet(*invalidation_set).SiblingDescendants()) {
      invalidation_lists.descendants.push_back(descendants);
    }
  }
  ScheduleInvalidationSetsForNode(invalidation_lists, *element.parentNode());
}

void PendingInvalidations::ClearInvalidation(ContainerNode& node) {
  if (!node.NeedsStyleInvalidation())
    return;
  pending_invalidation_map_.erase(&node);
  node.ClearNeedsStyleInvalidation();
}

NodeInvalidationSets& PendingInvalidations::EnsurePendingInvalidations(
    ContainerNode& node) {
  auto it = pending_invalidation_map_.find(&node);
  if (it != pending_invalidation_map_.end())
    return it->value;
  PendingInvalidationMap::AddResult add_result =
      pending_invalidation_map_.insert(&node, NodeInvalidationSets());
  return add_result.stored_value->value;
}

}  // namespace blink
