// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TRACE_WRAPPER_MEMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TRACE_WRAPPER_MEMBER_H_

#include "third_party/blink/renderer/platform/bindings/script_wrappable_marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

template <typename T>
class Member;

// TraceWrapperMember is used for Member fields that should participate in
// wrapper tracing, i.e., strongly hold a ScriptWrappable alive. All
// TraceWrapperMember fields must be traced in the class' |Trace| method.
template <class T>
class TraceWrapperMember : public Member<T> {
  DISALLOW_NEW();

 public:
  TraceWrapperMember() : Member<T>(nullptr) {}

  TraceWrapperMember(T* raw) : Member<T>(raw) {
    // We have to use a write barrier here because of in-place construction
    // in containers, such as HeapVector::push_back.
    ScriptWrappableMarkingVisitor::WriteBarrier(raw);
  }

  TraceWrapperMember(WTF::HashTableDeletedValueType x) : Member<T>(x) {}

  TraceWrapperMember(const TraceWrapperMember& other) { *this = other; }

  TraceWrapperMember& operator=(const TraceWrapperMember& other) {
    Member<T>::operator=(other);
    DCHECK_EQ(other.Get(), this->Get());
    ScriptWrappableMarkingVisitor::WriteBarrier(this->Get());
    return *this;
  }

  TraceWrapperMember& operator=(const Member<T>& other) {
    Member<T>::operator=(other);
    DCHECK_EQ(other.Get(), this->Get());
    ScriptWrappableMarkingVisitor::WriteBarrier(this->Get());
    return *this;
  }

  TraceWrapperMember& operator=(T* other) {
    Member<T>::operator=(other);
    DCHECK_EQ(other, this->Get());
    ScriptWrappableMarkingVisitor::WriteBarrier(this->Get());
    return *this;
  }

  TraceWrapperMember& operator=(std::nullptr_t) {
    // No need for a write barrier when assigning nullptr.
    Member<T>::operator=(nullptr);
    return *this;
  }
};

// Swaps two HeapVectors specialized for TraceWrapperMember. The custom swap
// function is required as TraceWrapperMember potentially requires emitting a
// write barrier.
template <typename T>
void swap(HeapVector<TraceWrapperMember<T>>& a,
          HeapVector<TraceWrapperMember<T>>& b) {
  // HeapVector<Member<T>> and HeapVector<TraceWrapperMember<T>> have the
  // same size and semantics.
  HeapVector<Member<T>>& a_ = reinterpret_cast<HeapVector<Member<T>>&>(a);
  HeapVector<Member<T>>& b_ = reinterpret_cast<HeapVector<Member<T>>&>(b);
  a_.swap(b_);
  if (ThreadState::IsAnyWrapperTracing() &&
      ThreadState::Current()->IsWrapperTracing()) {
    // If incremental marking is enabled we need to emit the write barrier since
    // the swap was performed on HeapVector<Member<T>>.
    for (auto item : a) {
      ScriptWrappableMarkingVisitor::WriteBarrier(item.Get());
    }
    for (auto item : b) {
      ScriptWrappableMarkingVisitor::WriteBarrier(item.Get());
    }
  }
}

// HeapVectorBacking<TraceWrapperMember<T>> need to map to
// HeapVectorBacking<Member<T>> for performing the swap method below.
template <typename T, typename Traits>
struct GCInfoTrait<HeapVectorBacking<TraceWrapperMember<T>, Traits>>
    : public GCInfoTrait<
          HeapVectorBacking<Member<T>, WTF::VectorTraits<Member<T>>>> {};

// Swaps two HeapVectors, one containing TraceWrapperMember and one with
// regular Members. The custom swap function is required as TraceWrapperMember
// potentially requires emitting a write barrier.
template <typename T>
void swap(HeapVector<TraceWrapperMember<T>>& a, HeapVector<Member<T>>& b) {
  // HeapVector<Member<T>> and HeapVector<TraceWrapperMember<T>> have the
  // same size and semantics. This cast and swap assumes that GCInfo for both
  // TraceWrapperMember and Member match in vector backings.
  HeapVector<Member<T>>& a_ = reinterpret_cast<HeapVector<Member<T>>&>(a);
  a_.swap(b);
  if (ThreadState::IsAnyWrapperTracing() &&
      ThreadState::Current()->IsWrapperTracing()) {
    // If incremental marking is enabled we need to emit the write barrier since
    // the swap was performed on HeapVector<Member<T>>.
    for (auto item : a) {
      ScriptWrappableMarkingVisitor::WriteBarrier(item.Get());
    }
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TRACE_WRAPPER_MEMBER_H_
