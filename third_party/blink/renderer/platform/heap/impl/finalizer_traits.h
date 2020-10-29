// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_FINALIZER_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_FINALIZER_TRAITS_H_

#include <type_traits>

#include "base/template_util.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {
namespace internal {

using FinalizationCallback = void (*)(void*);

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
#pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
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

// The FinalizerTrait is used to determine if a type requires finalization and
// what finalization means.
template <typename T>
struct FinalizerTrait {
  STATIC_ONLY(FinalizerTrait);

 private:
  static constexpr bool kNonTrivialFinalizer =
      internal::HasFinalizeGarbageCollectedObject<T>::value ||
      !std::is_trivially_destructible<typename std::remove_cv<T>::type>::value;

  static void Finalize(void* obj) {
    internal::FinalizerTraitImpl<T, kNonTrivialFinalizer>::Finalize(obj);
  }

 public:
  static constexpr FinalizationCallback kCallback =
      kNonTrivialFinalizer ? Finalize : nullptr;
};

}  // namespace internal
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_FINALIZER_TRAITS_H_
