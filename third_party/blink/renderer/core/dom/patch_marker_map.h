// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PATCH_MARKER_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PATCH_MARKER_MAP_H_

#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

class ContainerNode;
class ProcessingInstruction;

// Weak, add-only multimap of <?start name="..."> and <?marker name="...">
// processing instructions keyed by their `name` pseudo-attribute.
class PatchMarkerMap final : public GarbageCollected<PatchMarkerMap> {
 public:
  PatchMarkerMap() = default;

  void Add(ProcessingInstruction& marker);
  ProcessingInstruction* FindFirstWithin(const AtomicString& key,
                                         const ContainerNode& scope) const;

  void Trace(Visitor* visitor) const;

 private:
  using CandidateSet = GCedHeapHashSet<WeakMember<ProcessingInstruction>>;

  void ProcessCustomWeakness(const LivenessBroker& info);

  HeapHashMap<AtomicString, Member<CandidateSet>> markers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_PATCH_MARKER_MAP_H_
