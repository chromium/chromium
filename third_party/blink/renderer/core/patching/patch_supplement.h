// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_PATCH_SUPPLEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_PATCH_SUPPLEMENT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/patching/patch.h"
#include "third_party/blink/renderer/platform/graphics/dom_node_id.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class ScriptState;
class WritableStream;

class PatchSupplement : public GarbageCollected<PatchSupplement> {
 public:
  PatchSupplement() = default;

  // Supplement functionality.
  static PatchSupplement* From(Document&);
  static PatchSupplement* FromIfExists(const Document&);
  void Trace(Visitor*) const;

  Patch* CurrentPatchFor(const Node&);
  void DidStart(Node&, Patch*);
  void DidComplete(Node&);
  WritableStream* CreateSinglePatchStream(ScriptState*,
                                          ContainerNode& target,
                                          Node* previous_child,
                                          Node* next_child);
  WritableStream* CreateSubtreePatchStream(ScriptState*, ContainerNode& target);

 private:
  std::optional<size_t> IndexOfPatch(const Node& target);
  // TODO(nrosenthal): multiple patches per destination
  HeapVector<Member<Patch>> patches_;
};
}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PATCHING_PATCH_SUPPLEMENT_H_
