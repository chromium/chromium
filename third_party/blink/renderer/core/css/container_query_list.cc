// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_list.h"

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/style_recalc_context.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/event_target_names.h"

namespace blink {

ContainerQueryList::ContainerQueryList(ExecutionContext* context,
                                       ContainerQuery* container_query,
                                       Element* element)
    : ActiveScriptWrappable<ContainerQueryList>({}),
      ExecutionContextLifecycleObserver(context),
      container_query_(container_query),
      element_(element) {
  CHECK(element);
  CHECK(container_query);
}

ContainerQueryList::~ContainerQueryList() = default;

bool ContainerQueryList::matches() {
  element_->GetDocument().UpdateStyleAndLayoutForNode(
      element_, DocumentUpdateReason::kJavaScript);

  UpdateMatches();
  return matches_;
}

Element* ContainerQueryList::ResolveContainer() {
  Element* starting_element = FlatTreeTraversal::ParentElement(*element_);
  return ContainerQueryEvaluator::FindContainer(
      starting_element, container_query_->Selector(), nullptr);
}

void ContainerQueryList::UpdateMatches() {
  if (container_query_->Selector().HasUnknownFeature()) {
    matches_ = false;
    return;
  }

  container_ = ResolveContainer();
  if (!container_) {
    matches_ = false;
    return;
  }

  StyleRecalcContext context = StyleRecalcContext::FromAncestors(*container_);
  ContainerSelectorCache cache;
  MatchResult result;
  matches_ = ContainerQueryEvaluator::EvalAndAdd(
      container_, context, *container_query_, cache, result);
}

bool ContainerQueryList::HasPendingActivity() const {
  return HasEventListeners(event_type_names::kChange);
}

void ContainerQueryList::ContextDestroyed() {
  RemoveAllEventListeners();
}

void ContainerQueryList::Trace(Visitor* visitor) const {
  visitor->Trace(container_query_);
  visitor->Trace(element_);
  visitor->Trace(container_);
  EventTarget::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

const AtomicString& ContainerQueryList::InterfaceName() const {
  return event_target_names::kContainerQueryList;
}

ExecutionContext* ContainerQueryList::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

}  // namespace blink
