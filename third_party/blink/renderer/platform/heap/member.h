// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_

#include "third_party/blink/renderer/platform/wtf/buildflags.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

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

// PtrHash is the default hash for hash tables with Member<>-derived elements.
template <typename T>
struct MemberHash : PtrHash<T> {
  STATIC_ONLY(MemberHash);
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
struct DefaultHash<blink::Member<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::WeakMember<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct DefaultHash<blink::UntracedMember<T>> {
  STATIC_ONLY(DefaultHash);
  using Hash = MemberHash<T>;
};

template <typename T>
struct IsTraceable<blink::Member<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T>
struct IsWeak<blink::WeakMember<T>> : std::true_type {};

template <typename T>
struct IsTraceable<blink::WeakMember<T>> {
  STATIC_ONLY(IsTraceable);
  static const bool value = true;
};

template <typename T, typename MemberType>
struct BaseMemberHashTraits : SimpleClassHashTraits<MemberType> {
  STATIC_ONLY(BaseMemberHashTraits);

  using PeekInType = T*;
  using PeekOutType = T*;
  using IteratorGetType = MemberType*;
  using IteratorConstGetType = const MemberType*;
  using IteratorReferenceType = MemberType&;
  using IteratorConstReferenceType = const MemberType&;

  static PeekOutType Peek(const MemberType& value) { return value; }

  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }

  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  template <typename U>
  static void Store(const U& value, MemberType& storage) {
    storage = value;
  }

  static void ConstructDeletedValue(MemberType& slot, bool) {
#if BUILDFLAG(USE_V8_OILPAN)
    slot = cppgc::kSentinelPointer;
#else   // !USE_V8_OILPAN
    slot = WTF::kHashTableDeletedValue;
#endif  // !USE_V8_OILPAN
  }

  static bool IsDeletedValue(const MemberType& value) {
#if BUILDFLAG(USE_V8_OILPAN)
    return value.Get() == cppgc::kSentinelPointer;
#else   // !USE_V8_OILPAN
    return value.IsHashTableDeletedValue();
#endif  // !USE_V8_OILPAN
  }
};

template <typename T>
struct HashTraits<blink::Member<T>>
    : BaseMemberHashTraits<T, blink::Member<T>> {
  static constexpr bool kCanTraceConcurrently = true;
};

template <typename T>
struct HashTraits<blink::WeakMember<T>>
    : BaseMemberHashTraits<T, blink::WeakMember<T>> {
  static constexpr bool kCanTraceConcurrently = true;
};

template <typename T>
struct HashTraits<blink::UntracedMember<T>>
    : BaseMemberHashTraits<T, blink::UntracedMember<T>> {};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_MEMBER_H_
