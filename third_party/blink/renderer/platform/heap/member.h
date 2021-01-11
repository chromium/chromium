// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_

#include "third_party/blink/renderer/platform/wtf/buildflags.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

#if BUILDFLAG(USE_V8_OILPAN)
#include "third_party/blink/renderer/platform/heap/v8_wrapper/member.h"
#else  // !USE_V8_OILPAN
#include "third_party/blink/renderer/platform/heap/impl/member.h"
#endif  // !USE_V8_OILPAN

namespace blink {

template <typename T>
inline void swap(Member<T>& a, Member<T>& b) {
  a.Swap(b);
}

}  // namespace blink

namespace WTF {

template <typename T>
struct HashTraits<blink::Member<T>> : SimpleClassHashTraits<blink::Member<T>> {
  STATIC_ONLY(HashTraits);
  // FIXME: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = blink::Member<T>*;
  using IteratorConstGetType = const blink::Member<T>*;
  using IteratorReferenceType = blink::Member<T>&;
  using IteratorConstReferenceType = const blink::Member<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::Member<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::Member<T>& value) { return value; }

  static void ConstructDeletedValue(blink::Member<T>& slot, bool) {
    slot = WTF::kHashTableDeletedValue;
  }

  static constexpr bool kCanTraceConcurrently = true;
};

template <typename T>
struct HashTraits<blink::WeakMember<T>>
    : SimpleClassHashTraits<blink::WeakMember<T>> {
  STATIC_ONLY(HashTraits);
  static const bool kNeedsDestruction = false;
  // FIXME: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = blink::WeakMember<T>*;
  using IteratorConstGetType = const blink::WeakMember<T>*;
  using IteratorReferenceType = blink::WeakMember<T>&;
  using IteratorConstReferenceType = const blink::WeakMember<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::WeakMember<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::WeakMember<T>& value) { return value; }

  static void ConstructDeletedValue(blink::WeakMember<T>& slot, bool) {
    slot = WTF::kHashTableDeletedValue;
  }

  static constexpr bool kCanTraceConcurrently = true;
};

template <typename T>
struct HashTraits<blink::UntracedMember<T>>
    : SimpleClassHashTraits<blink::UntracedMember<T>> {
  STATIC_ONLY(HashTraits);
  static const bool kNeedsDestruction = false;
  // FIXME: Implement proper const'ness for iterator types.
  using PeekInType = T*;
  using IteratorGetType = blink::UntracedMember<T>*;
  using IteratorConstGetType = const blink::UntracedMember<T>*;
  using IteratorReferenceType = blink::UntracedMember<T>&;
  using IteratorConstReferenceType = const blink::UntracedMember<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }
  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::UntracedMember<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::UntracedMember<T>& value) {
    return value;
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_
