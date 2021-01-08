// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_

#include "third_party/blink/renderer/platform/wtf/buildflags.h"

#if BUILDFLAG(USE_V8_OILPAN)
#include "third_party/blink/renderer/platform/heap/v8_wrapper/persistent.h"
#else  // !USE_V8_OILPAN
#include "third_party/blink/renderer/platform/heap/impl/persistent.h"
#endif  // !USE_V8_OILPAN

namespace WTF {

template <
    typename T,
    blink::WeaknessPersistentConfiguration weaknessConfiguration,
    blink::CrossThreadnessPersistentConfiguration crossThreadnessConfiguration>
struct VectorTraits<blink::PersistentBase<T,
                                          weaknessConfiguration,
                                          crossThreadnessConfiguration>>
    : VectorTraitsBase<blink::PersistentBase<T,
                                             weaknessConfiguration,
                                             crossThreadnessConfiguration>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = true;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = false;
  static const bool kCanMoveWithMemcpy = true;
};

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

}  // namespace base

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_PERSISTENT_H_
