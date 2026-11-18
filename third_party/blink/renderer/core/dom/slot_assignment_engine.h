// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_ENGINE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_ENGINE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class ShadowRoot;

// Tracks the shadow roots of a document whose slot assignment is dirty. See
// SlotAssignment for the recalc model.
class CORE_EXPORT SlotAssignmentEngine final
    : public GarbageCollected<SlotAssignmentEngine> {
 public:
  explicit SlotAssignmentEngine();

  // Adds or removes a dirty shadow root. Connected and disconnected shadow
  // roots are tracked separately.
  void AddShadowRootNeedingRecalc(ShadowRoot&);
  void RemoveShadowRootNeedingRecalc(ShadowRoot&);

  void Connected(ShadowRoot&);
  void Disconnected(ShadowRoot&);

  // Only connected shadow roots count as pending; disconnected ones are
  // recalculated lazily.
  bool HasPendingSlotAssignmentRecalc() const {
    return !shadow_roots_needing_recalc_.empty();
  }

  // Returns the dirty shadow roots that must be recalculated together with
  // `shadow_root` (the "recalc list"), including `shadow_root` itself. For a
  // connected shadow root, this is every dirty connected shadow root in the
  // document. For a disconnected one, it is every dirty disconnected shadow
  // root with the same shadow-including root.
  HeapVector<Member<ShadowRoot>> CollectShadowRootsToRecalc(
      ShadowRoot& shadow_root);

  // Recalculates every dirty connected shadow root. Called from the document
  // lifecycle.
  void RecalcSlotAssignments();

  void Trace(Visitor*) const;

 private:
  HeapHashSet<WeakMember<ShadowRoot>> shadow_roots_needing_recalc_;
  HeapHashSet<WeakMember<ShadowRoot>> disconnected_shadow_roots_needing_recalc_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SLOT_ASSIGNMENT_ENGINE_H_
