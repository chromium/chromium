// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_TRACE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_TRACE_TRAITS_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/impl/gc_info.h"
#include "third_party/blink/renderer/platform/heap/impl/heap_page.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/list_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename T>
struct TraceTrait;

template <typename T, bool = NeedsAdjustPointer<T>::value>
struct AdjustPointerTrait;

template <typename T>
struct AdjustPointerTrait<T, false> {
  STATIC_ONLY(AdjustPointerTrait);

  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return {self, TraceTrait<T>::Trace};
  }
};

template <typename T>
struct AdjustPointerTrait<T, true> {
  STATIC_ONLY(AdjustPointerTrait);

  static TraceDescriptor GetTraceDescriptor(const void* self) {
    // Tracing an object, and more specifically GetTraceDescriptor for an
    // object, implies having a reference which means the object is at least in
    // construction. Therefore it is guaranteed that the ObjectStartBitmap was
    // already updated to include the object, and its HeapObjectHeader was
    // already created.
    HeapObjectHeader* const header = HeapObjectHeader::FromInnerAddress<
        HeapObjectHeader::AccessMode::kAtomic>(self);
    return {header->Payload(),
            GCInfo::From(
                header->GcInfoIndex<HeapObjectHeader::AccessMode::kAtomic>())
                .trace};
  }
};

template <typename T, bool = WTF::IsTraceable<T>::value>
struct TraceIfNeeded;

template <typename T>
struct TraceIfNeeded<T, false> {
  STATIC_ONLY(TraceIfNeeded);
  static void Trace(Visitor*, const T&) {}
};

template <typename T>
struct TraceIfNeeded<T, true> {
  STATIC_ONLY(TraceIfNeeded);
  static void Trace(Visitor* visitor, const T& t) { visitor->Trace(t); }
};

// The TraceTrait is used to specify how to trace and object for Oilpan and
// wrapper tracing.
//
//
// By default, the 'Trace' method implemented on an object itself is
// used to trace the pointers to other heap objects inside the object.
//
// However, the TraceTrait can be specialized to use a different
// implementation. A common case where a TraceTrait specialization is
// needed is when multiple inheritance leads to pointers that are not
// to the start of the object in the Blink garbage-collected heap. In
// that case the pointer has to be adjusted before marking.
template <typename T>
struct TraceTrait {
  STATIC_ONLY(TraceTrait);

 public:
  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return AdjustPointerTrait<T>::GetTraceDescriptor(
        static_cast<const T*>(self));
  }

  static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
    return {self, nullptr};
  }

  static void Trace(Visitor*, const void* self);
};

template <typename T>
struct TraceTrait<const T> : public TraceTrait<T> {};

template <typename T>
void TraceTrait<T>::Trace(Visitor* visitor, const void* self) {
  static_assert(WTF::IsTraceable<T>::value, "T should be traceable");
  static_cast<const T*>(self)->Trace(visitor);
}

// This trace trait for std::pair will null weak members if their referent is
// collected. If you have a collection that contain weakness it does not remove
// entries from the collection that contain nulled weak members.
template <typename T, typename U>
struct TraceTrait<std::pair<T, U>> {
  STATIC_ONLY(TraceTrait);

 public:
  static void Trace(Visitor* visitor, const std::pair<T, U>* pair) {
    TraceIfNeeded<T>::Trace(visitor, pair->first);
    TraceIfNeeded<U>::Trace(visitor, pair->second);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_TRACE_TRAITS_H_
