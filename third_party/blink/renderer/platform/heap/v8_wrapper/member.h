// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_MEMBER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_MEMBER_H_

#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/write_barrier.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/construct_traits.h"
#include "v8/include/cppgc/member.h"

namespace blink {

template <typename T>
using Member = cppgc::Member<T>;

template <typename T>
using WeakMember = cppgc::WeakMember<T>;

template <typename T>
using UntracedMember = cppgc::UntracedMember<T>;

template <typename T>
inline bool IsHashTableDeletedValue(const Member<T>& m) {
  return m == cppgc::kSentinelPointer;
}

constexpr auto kMemberDeletedValue = cppgc::kSentinelPointer;

template <typename T>
struct ThreadingTrait<blink::Member<T>> {
  STATIC_ONLY(ThreadingTrait);
  static constexpr ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T>
struct ThreadingTrait<blink::WeakMember<T>> {
  STATIC_ONLY(ThreadingTrait);
  static constexpr ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T>
struct ThreadingTrait<blink::UntracedMember<T>> {
  STATIC_ONLY(ThreadingTrait);
  static constexpr ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

}  // namespace blink

namespace WTF {

template <typename T, typename Traits, typename Allocator>
class MemberConstructTraits {
  STATIC_ONLY(MemberConstructTraits);

 public:
  template <typename... Args>
  static T* Construct(void* location, Args&&... args) {
    return new (NotNullTag::kNotNull, location) T(std::forward<Args>(args)...);
  }

  static void NotifyNewElement(T* element) {
    blink::WriteBarrier::DispatchForObject(element);
  }

  template <typename... Args>
  static T* ConstructAndNotifyElement(void* location, Args&&... args) {
    // ConstructAndNotifyElement updates an existing Member which might
    // also be comncurrently traced while we update it. The regular ctors
    // for Member don't use an atomic write which can lead to data races.
    T* object = Construct(location, std::forward<Args>(args)...,
                          typename T::AtomicInitializerTag());
    NotifyNewElement(object);
    return object;
  }

  static void NotifyNewElements(T* array, size_t len) {
    while (len-- > 0) {
      blink::WriteBarrier::DispatchForObject(array);
      array++;
    }
  }
};

template <typename T, typename Traits, typename Allocator>
class ConstructTraits<blink::Member<T>, Traits, Allocator>
    : public MemberConstructTraits<blink::Member<T>, Traits, Allocator> {};

template <typename T, typename Traits, typename Allocator>
class ConstructTraits<blink::WeakMember<T>, Traits, Allocator>
    : public MemberConstructTraits<blink::WeakMember<T>, Traits, Allocator> {};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_MEMBER_H_
