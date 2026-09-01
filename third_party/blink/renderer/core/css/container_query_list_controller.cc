// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/container_query_list_controller.h"

#include "third_party/blink/renderer/core/css/container_query_list.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

const char ContainerQueryListController::kSupplementName[] =
    "ContainerQueryListController";

ContainerQueryListController* ContainerQueryListController::From(
    LocalDOMWindow& window) {
  auto* controller = FromIfExists(window);
  if (!controller) {
    controller = MakeGarbageCollected<ContainerQueryListController>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, controller);
  }
  return controller;
}

ContainerQueryListController* ContainerQueryListController::FromIfExists(
    LocalDOMWindow& window) {
  return Supplement<LocalDOMWindow>::From<ContainerQueryListController>(window);
}

ContainerQueryListController::ContainerQueryListController(
    LocalDOMWindow& window)
    : Supplement(window) {}

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

bool ContainerQueryListController::NotifyChanges() {
  Document* document = GetSupplementable()->document();
  if (!document) {
    return false;
  }

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
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
