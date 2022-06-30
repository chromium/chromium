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
  LocalDOMWindow* window = document.domWindow();
  if (!window) {
    return;
  }

  LocalFrame* frame = window->GetFrame();
  if (!frame || !frame->IsMainFrame()) {
    return;
  }
  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  if (!script_state) {
    return;
  }

  SoftNavigationHeuristics* heuristics =
      SoftNavigationHeuristics::From(*window);
  DCHECK(heuristics);
  heuristics->ModifiedMain(script_state);
}

}  // namespace blink
