// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/render_blocking_element_link_map.h"

namespace blink {

RenderBlockingElementLinkMap::RenderBlockingElementLinkMap(
    RenderBlockingElementSetEmtpyCallback blocking_elements_set_empty_handler)
    : on_blocking_elements_empty_(blocking_elements_set_empty_handler) {}

RenderBlockingElementLinkMap::~RenderBlockingElementLinkMap() = default;

void RenderBlockingElementLinkMap::Trace(Visitor* visitor) const {
  visitor->Trace(element_render_blocking_links_);
}

void RenderBlockingElementLinkMap::AddLinkWithTargetElement(
    const AtomicString& target_element_id,
    const HTMLLinkElement* link,
    RenderBlockingLevel level) {
  // We don't add empty ids.
  if (target_element_id.empty()) {
    return;
  }
  auto it = element_render_blocking_links_.find(level);
  if (it == element_render_blocking_links_.end()) {
    auto result = element_render_blocking_links_.insert(
        level, MakeGarbageCollected<ElementLinkMap>());
    AddToElementLinkMap(result.stored_value->value.Get(), target_element_id,
                        link);
  } else {
    AddToElementLinkMap(it->value.Get(), target_element_id, link);
  }
}

void RenderBlockingElementLinkMap::AddToElementLinkMap(
    ElementLinkMap* element_link_map,
    const AtomicString& target_element_id,
    const HTMLLinkElement* link) {
  auto it = element_link_map->find(target_element_id);
  if (it == element_link_map->end()) {
    auto result = element_link_map->insert(
        target_element_id,
        MakeGarbageCollected<
            GCedHeapHashSet<WeakMember<const HTMLLinkElement>>>());
    result.stored_value->value->insert(link);
  } else {
    it->value->insert(link);
  }
}

void RenderBlockingElementLinkMap::RemoveTargetElement(
    const AtomicString& target_element_id) {
  // We don't add empty ids.
  if (target_element_id.empty()) {
    return;
  }
  for (auto& [level, element_link_map] : element_render_blocking_links_) {
    RemoveTargetElementFromElementLinkMap(element_link_map.Get(), level,
                                          target_element_id);
  }
}

void RenderBlockingElementLinkMap::RemoveTargetElementFromElementLinkMap(
    ElementLinkMap* element_link_map,
    RenderBlockingLevel level,
    const AtomicString& target_element_id) {
  if (element_link_map->empty() || target_element_id.empty()) {
    return;
  }

  element_link_map->erase(target_element_id);
  element_link_map->erase(
      AtomicString(EncodeWithURLEscapeSequences(target_element_id)));
  if (element_link_map->empty()) {
    on_blocking_elements_empty_.Run(level);
  }
}

void RenderBlockingElementLinkMap::RemoveLinkWithTargetElement(
    const AtomicString& target_element_id,
    const HTMLLinkElement* link) {
  // We don't add empty ids.
  if (target_element_id.empty()) {
    return;
  }
  for (auto& [level, element_link_map] : element_render_blocking_links_) {
    RemoveLinkFromElementLinkMap(element_link_map.Get(), level,
                                 target_element_id, link);
  }
}

void RenderBlockingElementLinkMap::RemoveLinkFromElementLinkMap(
    ElementLinkMap* element_link_map,
    RenderBlockingLevel level,
    const AtomicString& target_element_id,
    const HTMLLinkElement* link) {
  auto it = element_link_map->find(target_element_id);
  if (it == element_link_map->end()) {
    return;
  }

  it->value->erase(link);
  if (it->value->empty()) {
    element_link_map->erase(it);
  }

  if (element_link_map->empty()) {
    on_blocking_elements_empty_.Run(level);
  }
}

void RenderBlockingElementLinkMap::Clear() {
  element_render_blocking_links_.clear();
  for (int i = static_cast<int>(RenderBlockingLevel::kNone) + 1;
       i <= static_cast<int>(RenderBlockingLevel::kMax); i++) {
    on_blocking_elements_empty_.Run(static_cast<RenderBlockingLevel>(i));
  }
}

void RenderBlockingElementLinkMap::ForEach(
    base::RepeatingCallback<void(RenderBlockingLevel level,
                                 const HTMLLinkElement&)> func) {
  for (const auto& [level, element_link_map] : element_render_blocking_links_) {
    for (const auto& links : *element_link_map) {
      for (const auto& link : *links.value) {
        func.Run(level, *link);
      }
    }
  }
}

bool RenderBlockingElementLinkMap::HasElement(RenderBlockingLevel level) const {
  auto it = element_render_blocking_links_.find(level);
  if (it == element_render_blocking_links_.end()) {
    return false;
  }
  return !it->value->empty();
}

}  // namespace blink
