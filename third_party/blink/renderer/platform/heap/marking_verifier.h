// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VERIFIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VERIFIER_H_

#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// Marking verifier that checks that a child is marked if its parent is marked.
class MarkingVerifier final : public Visitor {
 public:
  explicit MarkingVerifier(ThreadState* state) : Visitor(state) {}
  ~MarkingVerifier() override = default;

  void VerifyObject(HeapObjectHeader* header);

  void Visit(void* object, TraceDescriptor desc) final;
  void VisitWeak(void* object,
                 void* object_weak_ref,
                 TraceDescriptor desc,
                 WeakCallback callback) final;

  void VisitBackingStoreStrongly(void*, void**, TraceDescriptor) final;

  void VisitBackingStoreWeakly(void*,
                               void**,
                               TraceDescriptor,
                               TraceDescriptor,
                               WeakCallback,
                               void*) final;

  // Unused overrides.
  void VisitBackingStoreOnly(void*, void**) final {}
  void RegisterBackingStoreCallback(void*, MovingObjectCallback) final {}
  void RegisterWeakCallback(WeakCallback, void*) final {}
  void Visit(const TraceWrapperV8Reference<v8::Value>&) final {}

 private:
  void VerifyChild(void* object, void* base_object_payload);

  HeapObjectHeader* parent_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MARKING_VERIFIER_H_
