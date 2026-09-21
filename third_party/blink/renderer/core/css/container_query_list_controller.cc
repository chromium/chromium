// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_list_controller.h"

#include "third_party/blink/renderer/core/css/container_query_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

const char ContainerQueryListController::kSupplementName[] =
    "ContainerQueryListController";

ContainerQueryListController* ContainerQueryListController::From(
    Document& document) {
  auto* controller = FromIfExists(document);
  if (!controller) {
    controller = MakeGarbageCollected<ContainerQueryListController>(document);
    Supplement<Document>::ProvideTo(document, controller);
  }
  return controller;
}

ContainerQueryListController* ContainerQueryListController::FromIfExists(
    Document& document) {
  return Supplement<Document>::From<ContainerQueryListController>(document);
}

ContainerQueryListController::ContainerQueryListController(Document& document)
    : Supplement(document) {}

void ContainerQueryListController::AddContainerQueryList(
    Element& element,
    ContainerQueryList& list) {
  elements_.insert(&element);
  auto add_result = lists_by_element_.insert(&element, nullptr);
  if (add_result.is_new_entry) {
    add_result.stored_value->value = MakeGarbageCollected<ListSet>();
  }
  add_result.stored_value->value->insert(&list);
}

void ContainerQueryListController::InvalidateSelectorCache(Document& document) {
  if (!RuntimeEnabledFeatures::ElementMatchContainerEnabled()) {
    return;
  }
  if (auto* controller = FromIfExists(document)) {
    ++controller->selector_cache_generation_;
  }
}

void ContainerQueryListController::InvalidateSelectorCacheFor(
    Element& element) {
  if (!RuntimeEnabledFeatures::ElementMatchContainerEnabled()) {
    return;
  }
  auto* controller = FromIfExists(element.GetDocument());
  if (!controller) {
    return;
  }
  auto it = controller->lists_by_element_.find(&element);
  if (it == controller->lists_by_element_.end()) {
    return;
  }
  for (ContainerQueryList* list : *it->value) {
    list->MarkCacheStale();
  }
}

bool ContainerQueryListController::NotifyChanges() {
  bool dispatched = false;
  HeapVector<Member<Element>> elements(elements_);
  for (Element* element : elements) {
    auto it = lists_by_element_.find(element);
    if (it == lists_by_element_.end()) {
      continue;
    }
    HeapVector<Member<ContainerQueryList>> lists(*it->value);
    for (ContainerQueryList* list : lists) {
      if (list->UpdateMatches() && list->HasPendingActivity()) {
        list->DispatchEvent(*Event::Create(event_type_names::kChange));
        dispatched = true;
      }
    }
  }
  return dispatched;
}

void ContainerQueryListController::Trace(Visitor* visitor) const {
  visitor->Trace(elements_);
  visitor->Trace(lists_by_element_);
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
