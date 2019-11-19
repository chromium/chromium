// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_FINALIZER_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_FINALIZER_TRAITS_H_

#include <type_traits>

#include "base/template_util.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace WTF {

template <typename ValueArg, typename Allocator>
class ListHashSetNode;

}  // namespace WTF

namespace blink {

namespace internal {

template <typename T, typename = void>
struct HasFinalizeGarbageCollectedObject : std::false_type {};

template <typename T>
struct HasFinalizeGarbageCollectedObject<
    T,
    base::void_t<decltype(std::declval<T>().FinalizeGarbageCollectedObject())>>
    : std::true_type {};

// The FinalizerTraitImpl specifies how to finalize objects.
template <typename T, bool isFinalized>
struct FinalizerTraitImpl;

template <typename T>
struct FinalizerTraitImpl<T, true> {
 private:
  STATIC_ONLY(FinalizerTraitImpl);
  struct Custom {
    static void Call(void* obj) {
      static_cast<T*>(obj)->FinalizeGarbageCollectedObject();
    }
  };
  struct Destructor {
    static void Call(void* obj) {
// The garbage collector differs from regular C++ here as it remembers whether
// an object's base class has a virtual destructor. In case there is no virtual
// destructor present, the object is always finalized through its leaf type. In
// other words: there is no finalization through a base pointer.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdelete-non-abstract-non-virtual-dtor"
      static_cast<T*>(obj)->~T();
#pragma GCC diagnostic pop
    }
  };
  using FinalizeImpl =
      std::conditional_t<HasFinalizeGarbageCollectedObject<T>::value,
                         Custom,
                         Destructor>;

 public:
  static void Finalize(void* obj) {
    static_assert(sizeof(T), "T must be fully defined");
    FinalizeImpl::Call(obj);
  }
};

template <typename T>
struct FinalizerTraitImpl<T, false> {
  STATIC_ONLY(FinalizerTraitImpl);
  static void Finalize(void* obj) {
    static_assert(sizeof(T), "T must be fully defined");
  }
};

}  // namespace internal

// The FinalizerTrait is used to determine if a type requires finalization and
// what finalization means.
template <typename T>
struct FinalizerTrait {
  STATIC_ONLY(FinalizerTrait);
  static constexpr bool kNonTrivialFinalizer =
      internal::HasFinalizeGarbageCollectedObject<T>::value ||
      !std::is_trivially_destructible<typename std::remove_cv<T>::type>::value;
  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<T, kNonTrivialFinalizer>::Finalize(obj);
  }
};

class HeapAllocator;
template <typename T, typename Traits>
class HeapVectorBacking;
template <typename Table>
class HeapHashTableBacking;

template <typename T, typename U, typename V>
struct FinalizerTrait<LinkedHashSet<T, U, V, HeapAllocator>> {
  STATIC_ONLY(FinalizerTrait);
  static constexpr bool kNonTrivialFinalizer = true;
  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<LinkedHashSet<T, U, V, HeapAllocator>,
                                 kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename T, typename Allocator>
struct FinalizerTrait<WTF::ListHashSetNode<T, Allocator>> {
  STATIC_ONLY(FinalizerTrait);
  static constexpr bool kNonTrivialFinalizer =
      !std::is_trivially_destructible<T>::value;
  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<WTF::ListHashSetNode<T, Allocator>,
                                 kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename T, size_t inlineCapacity>
struct FinalizerTrait<Vector<T, inlineCapacity, HeapAllocator>> {
  STATIC_ONLY(FinalizerTrait);
  static constexpr bool kNonTrivialFinalizer =
      inlineCapacity && VectorTraits<T>::kNeedsDestruction;
  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<Vector<T, inlineCapacity, HeapAllocator>,
                                 kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename T, size_t inlineCapacity>
struct FinalizerTrait<Deque<T, inlineCapacity, HeapAllocator>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer =
      inlineCapacity && VectorTraits<T>::kNeedsDestruction;
  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<Deque<T, inlineCapacity, HeapAllocator>,
                                 kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename Table>
struct FinalizerTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer =
      !std::is_trivially_destructible<typename Table::ValueType>::value;
  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<HeapHashTableBacking<Table>,
                                 kNonTrivialFinalizer>::Finalize(obj);
  }
};

template <typename T, typename Traits>
struct FinalizerTrait<HeapVectorBacking<T, Traits>> {
  STATIC_ONLY(FinalizerTrait);
  static const bool kNonTrivialFinalizer = Traits::kNeedsDestruction;
  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<HeapVectorBacking<T, Traits>,
                                 kNonTrivialFinalizer>::Finalize(obj);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_FINALIZER_TRAITS_H_
