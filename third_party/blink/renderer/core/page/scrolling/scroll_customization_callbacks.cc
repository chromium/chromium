// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/scroll_customization_callbacks.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/page/scrolling/scroll_state_callback.h"

namespace blink {

void ScrollCustomizationCallbacks::SetDistributeScroll(
    Node* node,
    ScrollStateCallback* scroll_state_callback) {
  distribute_scroll_callbacks_.Set(node, scroll_state_callback);
}

ScrollStateCallback* ScrollCustomizationCallbacks::GetDistributeScroll(
    Node* node) {
  auto it = distribute_scroll_callbacks_.find(node);
  if (it == distribute_scroll_callbacks_.end())
    return nullptr;
  return it->value.Get();
}

void ScrollCustomizationCallbacks::SetApplyScroll(
    Node* node,
    ScrollStateCallback* scroll_state_callback) {
  apply_scroll_callbacks_.Set(node, scroll_state_callback);
}

void ScrollCustomizationCallbacks::RemoveApplyScroll(Node* node) {
  apply_scroll_callbacks_.erase(node);
}

ScrollStateCallback* ScrollCustomizationCallbacks::GetApplyScroll(Node* node) {
  auto it = apply_scroll_callbacks_.find(node);
  if (it == apply_scroll_callbacks_.end())
    return nullptr;
  return it->value.Get();
}

bool ScrollCustomizationCallbacks::InScrollPhase(Node* node) const {
  return in_scrolling_phase_.Contains(node) && in_scrolling_phase_.at(node);
}

void ScrollCustomizationCallbacks::SetInScrollPhase(Node* node, bool value) {
  DCHECK(node);
  in_scrolling_phase_.Set(node, value);
}

}  // namespace blink
