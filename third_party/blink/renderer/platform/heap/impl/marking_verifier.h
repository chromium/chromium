// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_MARKING_VERIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_MARKING_VERIFIER_H_

#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

class HeapObjectHeader;

// Marking verifier that checks that a child is marked if its parent is marked.
class MarkingVerifier final : public Visitor {
 public:
  explicit MarkingVerifier(ThreadState* state) : Visitor(state) {}
  ~MarkingVerifier() override = default;

  void VerifyObject(HeapObjectHeader* header);

  void Visit(const void* object, TraceDescriptor desc) final;
  void VisitWeak(const void* object,
                 const void* object_weak_ref,
                 TraceDescriptor desc,
                 WeakCallback callback) final;

  void VisitWeakContainer(const void*,
                          const void* const*,
                          TraceDescriptor,
                          TraceDescriptor,
                          WeakCallback,
                          const void*) final;

 private:
  void VerifyChild(const void* object, const void* base_object_payload);

  HeapObjectHeader* parent_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_MARKING_VERIFIER_H_
