// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_list.h"

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/container_query_set.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/style_recalc_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/event_target_names.h"

namespace blink {

ContainerQueryList::ContainerQueryList(
    ExecutionContext* context,
    const ContainerQuerySet* container_query_set,
    Element* element)
    : ActiveScriptWrappable<ContainerQueryList>({}),
      ExecutionContextLifecycleObserver(context),
      container_query_set_(container_query_set),
      element_(element) {
  CHECK(element);
}

ContainerQueryList::~ContainerQueryList() = default;

bool ContainerQueryList::matches() {
  element_->GetDocument().UpdateStyleAndLayoutForNode(
      element_, DocumentUpdateReason::kJavaScript);

  UpdateMatches();
  return matches_;
}

String ContainerQueryList::query() const {
  if (!container_query_set_) {
    return String();
  }
  return container_query_set_->ToString();
}

void ContainerQueryList::UpdateMatches() {
  matches_ = false;

  if (!container_query_set_) {
    return;
  }

  Element* starting_element = FlatTreeTraversal::ParentElement(*element_);
  ContainerSelectorCache cache;
  MatchResult result;
  for (const ContainerQuery* container_query :
       container_query_set_->Queries()) {
    if (container_query->Selector().HasUnknownFeature()) {
      continue;
    }

    if (ContainerQueryEvaluator::EvalAndAdd(starting_element,
                                            StyleRecalcContext(),
                                            *container_query, cache, result)) {
      matches_ = true;
      break;
    }
  }
}

bool ContainerQueryList::HasPendingActivity() const {
  return HasEventListeners(event_type_names::kChange);
}

void ContainerQueryList::ContextDestroyed() {
  RemoveAllEventListeners();
}

void ContainerQueryList::Trace(Visitor* visitor) const {
  visitor->Trace(container_query_set_);
  visitor->Trace(element_);
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
