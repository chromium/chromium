// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_

#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"
#include "v8/include/cppgc/cross-thread-persistent.h"
#include "v8/include/cppgc/persistent.h"
#include "v8/include/cppgc/source-location.h"

// Required to optimize away locations for builds that do not need them to avoid
// binary size blowup.
#if BUILDFLAG(VERBOSE_PERSISTENT)
#define PERSISTENT_LOCATION_FROM_HERE blink::PersistentLocation::Current()
#else  // !BUILDFLAG(VERBOSE_PERSISTENT)
#define PERSISTENT_LOCATION_FROM_HERE blink::PersistentLocation()
#endif  // !BUILDFLAG(VERBOSE_PERSISTENT)

namespace blink {

template <typename T>
using Persistent = cppgc::Persistent<T>;

template <typename T>
using WeakPersistent = cppgc::WeakPersistent<T>;

// CrossThreadPersistent allows retaining objects from threads other than the
// thread that owns the heap of the corresponding object.
//
// Strongly prefer using `CrossThreadHandle` if the use case allows.
//
// Caveats:
// - Does not protect the heap owning an object from terminating. E.g., posting
//   a task with a CrossThreadPersistent for `this` will result in a
//   use-after-free in case the heap owning `this` is terminated before the task
//   is invoked.
// - Reaching transitively through the graph is unsupported as objects may be
//   moved concurrently on the thread owning the object.
// template <typename T>
// using CrossThreadPersistent = cppgc::subtle::CrossThreadPersistent<T>;

// CrossThreadWeakPersistent allows weakly retaining objects from threads other
// than the thread that owns the heap of the corresponding object.
//
// Strongly prefer using `CrossThreadWeakHandle` if the use case allows.
//
// Caveats:
// - Does not protect the heap owning an object from termination, as the
//   reference is weak.
// - In order to access the underlying object
//   `CrossThreadWeakPersistent<T>::Lock()` must be used which returns a
//   `CrossThreadPersistent<T>` which in turn also does not protect the heap
//   owning the object from terminating (see above).
// - Reaching transitively through the graph is unsupported as objects may be
//   moved concurrently on the thread owning the object.
// template <typename T>
// using CrossThreadWeakPersistent =
// cppgc::subtle::WeakCrossThreadPersistent<T>;

using PersistentLocation = cppgc::SourceLocation;

template <typename T>
Persistent<T> WrapPersistent(
    T* value,
    const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE) {
  return Persistent<T>(value, loc);
}

template <typename T>
WeakPersistent<T> WrapWeakPersistent(
    T* value,
    const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE) {
  return WeakPersistent<T>(value, loc);
}

// template <typename T>
// CrossThreadPersistent<T> WrapCrossThreadPersistent(
//     T* value,
//     const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE) {
//   return CrossThreadPersistent<T>(value, loc);
// }

// template <typename T>
// CrossThreadWeakPersistent<T> WrapCrossThreadWeakPersistent(
//     T* value,
//     const PersistentLocation& loc = PERSISTENT_LOCATION_FROM_HERE) {
//   return CrossThreadWeakPersistent<T>(value, loc);
// }

template <typename U, typename T, typename weakness>
cppgc::internal::BasicPersistent<U, weakness> DownCast(
    const cppgc::internal::BasicPersistent<T, weakness>& p) {
  return p.template To<U>();
}

template <typename U, typename T, typename weakness>
cppgc::internal::BasicCrossThreadPersistent<U, weakness> DownCast(
    const cppgc::internal::BasicCrossThreadPersistent<T, weakness>& p) {
  return p.template To<U>();
}

template <typename T,
          typename = std::enable_if_t<WTF::IsGarbageCollectedType<T>::value>>
Persistent<T> WrapPersistentIfNeeded(T* value) {
  return Persistent<T>(value);
}

template <typename T>
T& WrapPersistentIfNeeded(T& value) {
  return value;
}

}  // namespace blink

namespace WTF {

template <typename T>
struct PersistentVectorTraitsBase : VectorTraitsBase<T> {
  STATIC_ONLY(PersistentVectorTraitsBase);
  static const bool kCanInitializeWithMemset = true;
};

template <typename T>
struct VectorTraits<blink::Persistent<T>>
    : PersistentVectorTraitsBase<blink::Persistent<T>> {};

template <typename T>
struct VectorTraits<blink::WeakPersistent<T>>
    : PersistentVectorTraitsBase<blink::WeakPersistent<T>> {};

// template <typename T>
// struct VectorTraits<blink::CrossThreadPersistent<T>>
//     : PersistentVectorTraitsBase<blink::CrossThreadPersistent<T>> {};

// template <typename T>
// struct VectorTraits<blink::CrossThreadWeakPersistent<T>>
//     : PersistentVectorTraitsBase<blink::CrossThreadWeakPersistent<T>> {};

template <typename T, typename PersistentType>
struct BasePersistentHashTraits : SimpleClassHashTraits<PersistentType> {
  STATIC_ONLY(BasePersistentHashTraits);

  // TODO: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = PersistentType*;
  using IteratorConstGetType = const PersistentType*;
  using IteratorReferenceType = PersistentType&;
  using IteratorConstReferenceType = const PersistentType&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, PersistentType& storage) {
    storage = value;
  }

  static PeekOutType Peek(const PersistentType& value) { return value; }

  static void ConstructDeletedValue(PersistentType& slot, bool) {
    new (&slot) PersistentType(cppgc::kSentinelPointer);
  }

  static bool IsDeletedValue(const PersistentType& value) {
    return value.Get() == cppgc::kSentinelPointer;
  }
};

template <typename T>
struct HashTraits<blink::Persistent<T>>
    : BasePersistentHashTraits<T, blink::Persistent<T>> {};

template <typename T>
struct HashTraits<blink::WeakPersistent<T>>
    : BasePersistentHashTraits<T, blink::WeakPersistent<T>> {};

// template <typename T>
// struct HashTraits<blink::CrossThreadPersistent<T>>
//     : BasePersistentHashTraits<T, blink::CrossThreadPersistent<T>> {};

// template <typename T>
// struct HashTraits<blink::CrossThreadWeakPersistent<T>>
//     : BasePersistentHashTraits<T, blink::CrossThreadWeakPersistent<T>> {};

// Default hash for hash tables with Persistent<>-derived elements.
template <typename T>
struct PersistentHashBase : PtrHash<T> {
  STATIC_ONLY(PersistentHashBase);

  template <typename U>
  static unsigned GetHash(const U& key) {
    return PtrHash<T>::GetHash(key);
  }

  template <typename U, typename V>
  static bool Equal(const U& a, const V& b) {
    return a == b;
  }
};

template <typename T>
struct DefaultHash<blink::Persistent<T>> : PersistentHashBase<T> {};

template <typename T>
struct DefaultHash<blink::WeakPersistent<T>> : PersistentHashBase<T> {};

// template <typename T>
// struct DefaultHash<blink::CrossThreadPersistent<T>> : PersistentHashBase<T>
// {};

// template <typename T>
// struct DefaultHash<blink::CrossThreadWeakPersistent<T>> :
// PersistentHashBase<T> {};

// template <typename T>
// struct CrossThreadCopier<blink::CrossThreadPersistent<T>>
//     : public CrossThreadCopierPassThrough<blink::CrossThreadPersistent<T>> {
//   STATIC_ONLY(CrossThreadCopier);
// };

// template <typename T>
// struct CrossThreadCopier<blink::CrossThreadWeakPersistent<T>>
//     : public
//     CrossThreadCopierPassThrough<blink::CrossThreadWeakPersistent<T>> {
//   STATIC_ONLY(CrossThreadCopier);
// };

}  // namespace WTF

namespace base {

template <typename T>
struct IsWeakReceiver;

template <typename T>
struct IsWeakReceiver<blink::WeakPersistent<T>> : std::true_type {};

// template <typename T>
// struct IsWeakReceiver<blink::CrossThreadWeakPersistent<T>> : std::true_type
// {};

// template <typename T>
// struct BindUnwrapTraits<blink::CrossThreadWeakPersistent<T>> {
//   static blink::CrossThreadPersistent<T> Unwrap(
//       const blink::CrossThreadWeakPersistent<T>& wrapped) {
//     return wrapped.Lock();
//   }
// };

template <typename>
struct MaybeValidTraits;

// TODO(https://crbug.com/653394): Consider returning a thread-safe best
// guess of validity. MaybeValid() can be invoked from an arbitrary thread.
template <typename T>
struct MaybeValidTraits<blink::WeakPersistent<T>> {
  static bool MaybeValid(const blink::WeakPersistent<T>& p) { return true; }
};

// template <typename T>
// struct MaybeValidTraits<blink::CrossThreadWeakPersistent<T>> {
//   static bool MaybeValid(const blink::CrossThreadWeakPersistent<T>& p) {
//     return true;
//   }
// };

}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
