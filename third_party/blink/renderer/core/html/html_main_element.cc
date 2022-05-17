// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_main_element.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/timing/soft_navigation_heuristics.h"

namespace blink {

HTMLMainElement::HTMLMainElement(Document& document)
    : HTMLElement(html_names::kMainTag, document) {}

void HTMLMainElement::ChildrenChanged(const ChildrenChange& children_change) {
  HTMLElement::ChildrenChanged(children_change);
  if (!children_change.ByParser()) {
    NotifySoftNavigationHeuristics();
  }
}

Node::InsertionNotificationRequest HTMLMainElement::InsertedInto(
    ContainerNode& container_node) {
  // Here we don't really know that the insertion was API driven rather than
  // parser driven, but the overhead is minimal and it won't result in
  // correctness issues.
  NotifySoftNavigationHeuristics();
  return HTMLElement::InsertedInto(container_node);
}

void HTMLMainElement::NotifySoftNavigationHeuristics() {
  const Document& document = GetDocument();
  if (LocalDOMWindow* window = document.domWindow()) {
    if (LocalFrame* frame = window->GetFrame()) {
      if (frame->IsMainFrame()) {
        if (ScriptState* script_state = ToScriptStateForMainWorld(frame)) {
          SoftNavigationHeuristics* heuristics =
              SoftNavigationHeuristics::From(*window);
          heuristics->ModifiedDOM(script_state);
        }
      }
    }
  }
}
}  // namespace blink
