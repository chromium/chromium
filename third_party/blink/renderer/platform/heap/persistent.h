// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_

#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/buildflags.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

#if BUILDFLAG(USE_V8_OILPAN)
#include "third_party/blink/renderer/platform/heap/v8_wrapper/persistent.h"
#else  // !USE_V8_OILPAN
#include "third_party/blink/renderer/platform/heap/impl/persistent.h"
#endif  // !USE_V8_OILPAN

namespace blink {

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

template <typename T>
struct VectorTraits<blink::CrossThreadPersistent<T>>
    : PersistentVectorTraitsBase<blink::CrossThreadPersistent<T>> {};

template <typename T>
struct VectorTraits<blink::CrossThreadWeakPersistent<T>>
    : PersistentVectorTraitsBase<blink::CrossThreadWeakPersistent<T>> {};

template <typename T, typename H>
struct HandleHashTraits : SimpleClassHashTraits<H> {
  STATIC_ONLY(HandleHashTraits);
  // TODO: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = H*;
  using IteratorConstGetType = const H*;
  using IteratorReferenceType = H&;
  using IteratorConstReferenceType = const H&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, H& storage) {
    storage = value;
  }

  static PeekOutType Peek(const H& value) { return value; }
};

template <typename T>
struct HashTraits<blink::Persistent<T>>
    : HandleHashTraits<T, blink::Persistent<T>> {};

template <typename T>
struct HashTraits<blink::CrossThreadPersistent<T>>
    : HandleHashTraits<T, blink::CrossThreadPersistent<T>> {};

template <typename T>
struct DefaultHash<blink::Persistent<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::WeakPersistent<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::CrossThreadPersistent<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::CrossThreadWeakPersistent<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct CrossThreadCopier<blink::CrossThreadPersistent<T>>
    : public CrossThreadCopierPassThrough<blink::CrossThreadPersistent<T>> {
  STATIC_ONLY(CrossThreadCopier);
};

template <typename T>
struct CrossThreadCopier<blink::CrossThreadWeakPersistent<T>>
    : public CrossThreadCopierPassThrough<blink::CrossThreadWeakPersistent<T>> {
  STATIC_ONLY(CrossThreadCopier);
};

}  // namespace WTF

namespace base {

template <typename T>
struct IsWeakReceiver<blink::WeakPersistent<T>> : std::true_type {};

template <typename T>
struct IsWeakReceiver<blink::CrossThreadWeakPersistent<T>> : std::true_type {};

template <typename T>
struct BindUnwrapTraits<blink::CrossThreadWeakPersistent<T>> {
  static blink::CrossThreadPersistent<T> Unwrap(
      const blink::CrossThreadWeakPersistent<T>& wrapped) {
    return blink::CrossThreadPersistent<T>(wrapped);
  }
};

// TODO(https://crbug.com/653394): Consider returning a thread-safe best
// guess of validity. MaybeValid() can be invoked from an arbitrary thread.
template <typename T>
struct MaybeValidTraits<blink::WeakPersistent<T>> {
  static bool MaybeValid(const blink::WeakPersistent<T>& p) { return true; }
};

template <typename T>
struct MaybeValidTraits<blink::CrossThreadWeakPersistent<T>> {
  static bool MaybeValid(const blink::CrossThreadWeakPersistent<T>& p) {
    return true;
  }
};

}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
