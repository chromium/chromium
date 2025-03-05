// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/render_blocking_element_link_map.h"

namespace blink {

RenderBlockingElementLinkMap::RenderBlockingElementLinkMap(
    base::RepeatingClosure blocking_elmenents_empty_handler)
    : on_blocking_elements_empty_(blocking_elmenents_empty_handler) {}

RenderBlockingElementLinkMap::~RenderBlockingElementLinkMap() = default;

void RenderBlockingElementLinkMap::Trace(Visitor* visitor) const {
  visitor->Trace(element_render_blocking_links_);
}

void RenderBlockingElementLinkMap::AddLinkWithTargetElement(
    const AtomicString& target_element_id,
    const HTMLLinkElement* link) {
  auto it = element_render_blocking_links_.find(target_element_id);
  if (it == element_render_blocking_links_.end()) {
    auto result = element_render_blocking_links_.insert(
        target_element_id,
        MakeGarbageCollected<HeapHashSet<WeakMember<const HTMLLinkElement>>>());
    result.stored_value->value->insert(link);
  } else {
    it->value->insert(link);
  }
}

void RenderBlockingElementLinkMap::RemoveTargetElement(
    const AtomicString& target_element_id) {
  if (element_render_blocking_links_.empty() || target_element_id.empty()) {
    return;
  }

  element_render_blocking_links_.erase(target_element_id);
  element_render_blocking_links_.erase(
      AtomicString(EncodeWithURLEscapeSequences(target_element_id)));
  if (element_render_blocking_links_.empty()) {
    on_blocking_elements_empty_.Run();
  }
}

void RenderBlockingElementLinkMap::RemoveLinkWithTargetElement(
    const AtomicString& target_element_id,
    const HTMLLinkElement* link) {
  // We don't add empty ids.
  if (target_element_id.empty()) {
    return;
  }

  auto it = element_render_blocking_links_.find(target_element_id);
  if (it == element_render_blocking_links_.end()) {
    return;
  }

  it->value->erase(link);
  if (it->value->empty()) {
    element_render_blocking_links_.erase(it);
  }

  if (element_render_blocking_links_.empty()) {
    on_blocking_elements_empty_.Run();
  }
}

void RenderBlockingElementLinkMap::Clear() {
  element_render_blocking_links_.clear();
  on_blocking_elements_empty_.Run();
}

void RenderBlockingElementLinkMap::ForEach(
    base::RepeatingCallback<void(const HTMLLinkElement&)> func) {
  for (const auto& links : element_render_blocking_links_) {
    for (const auto& link : *(links.value)) {
      func.Run(*link);
    }
  }
}

}  // namespace blink
