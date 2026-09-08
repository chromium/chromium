// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_list.h"

#include "third_party/blink/renderer/core/css/container_query.h"
#include "third_party/blink/renderer/core/css/container_query_evaluator.h"
#include "third_party/blink/renderer/core/css/container_query_list_controller.h"
#include "third_party/blink/renderer/core/css/container_query_set.h"
#include "third_party/blink/renderer/core/css/resolver/match_result.h"
#include "third_party/blink/renderer/core/css/style_recalc_context.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

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

  if (!evaluated_) {
    UpdateMatches();
    return matches_;
  }

  return ComputeMatches();
}

bool ContainerQueryList::UpdateMatches() {
  bool current = ComputeMatches();
  bool changed = evaluated_ && current != matches_;
  matches_ = current;
  evaluated_ = true;
  return changed;
}

String ContainerQueryList::query() const {
  if (!container_query_set_) {
    return String();
  }
  return container_query_set_->ToString();
}

bool ContainerQueryList::ComputeMatches() {
  if (!container_query_set_) {
    return false;
  }

  InvalidateCacheIfStale();

  Element* starting_element = FlatTreeTraversal::ParentElement(*element_);
  MatchResult result;

  const ComputedStyle* style =
      ComputedStyle::NullifyEnsured(element_->GetComputedStyle());

  // selector_cache_ is invalidated from Element::DetachLayoutTree(), which
  // does not reach elements in display:none subtrees.
  // Do not cache for such elements; evaluate them with a local cache instead.
  ContainerSelectorCache local_cache;

  for (const ContainerQuery* container_query :
       container_query_set_->Queries()) {
    if (container_query->Selector().HasUnknownFeature()) {
      continue;
    }

    if (ContainerQueryEvaluator::EvalAndAdd(
            starting_element, StyleRecalcContext(), *container_query,
            style ? selector_cache_ : local_cache, result)) {
      return true;
    }
  }

  return false;
}

void ContainerQueryList::InvalidateCacheIfStale() {
  uint64_t cache_generation = 0;
  if (LocalDOMWindow* window = element_->GetDocument().domWindow()) {
    if (auto* controller =
            ContainerQueryListController::FromIfExists(*window)) {
      cache_generation = controller->SelectorCacheGeneration();
    }
  }
  if (cache_generation == 0 || !selector_cache_generation_.has_value() ||
      selector_cache_generation_ != cache_generation) {
    selector_cache_.clear();
  }
  selector_cache_generation_ = cache_generation;
}

void ContainerQueryList::MarkCacheStale() {
  selector_cache_generation_.reset();
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
  visitor->Trace(selector_cache_);
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
