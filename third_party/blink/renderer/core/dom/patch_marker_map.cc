// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/patch_marker_map.h"

#include <algorithm>

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/processing_instruction.h"

namespace blink {

void PatchMarkerMap::Add(ProcessingInstruction& marker) {
  const AtomicString& key = marker.PatchMarkerName();
  if (key.empty()) {
    return;
  }
  auto add_result = markers_.insert(key, nullptr);
  if (add_result.is_new_entry) {
    add_result.stored_value->value = MakeGarbageCollected<CandidateSet>();
  }
  add_result.stored_value->value->insert(&marker);
}

ProcessingInstruction* PatchMarkerMap::FindFirstWithin(
    const AtomicString& key,
    const ContainerNode& scope) const {
  if (key.empty()) {
    return nullptr;
  }
  auto it = markers_.find(key);
  if (it == markers_.end()) {
    return nullptr;
  }
  ProcessingInstruction* first = nullptr;
  for (ProcessingInstruction* candidate : *it->value) {
    if (candidate->PatchMarkerName() == key &&
        candidate->IsDescendantOf(&scope) &&
        (!first || (first->compareDocumentPosition(candidate) &
                    Node::kDocumentPositionPreceding))) {
      first = candidate;
    }
  }
  return first;
}

void PatchMarkerMap::ProcessCustomWeakness(const LivenessBroker& info) {
  markers_.erase_if([&info](const auto& pair) {
    return std::ranges::none_of(*pair.value, [&info](const auto& marker) {
      return info.IsHeapObjectAlive(marker);
    });
  });
}

void PatchMarkerMap::Trace(Visitor* visitor) const {
  visitor->Trace(markers_);
  visitor->RegisterWeakCallbackMethod<PatchMarkerMap,
                                      &PatchMarkerMap::ProcessCustomWeakness>(
      this);
}

}  // namespace blink
