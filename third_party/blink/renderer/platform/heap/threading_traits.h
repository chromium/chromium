// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREADING_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_THREADING_TRAITS_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// ThreadAffinity indicates which threads objects can be used on. We
// distinguish between objects that can be used on the main thread
// only and objects that can be used on any thread.
//
// For objects that can only be used on the main thread, we avoid going
// through thread-local storage to get to the thread state. This is
// important for performance.
enum ThreadAffinity {
  kAnyThread,
  kMainThreadOnly,
};

// TODO(haraken): These forward declarations violate dependency rules.
// Remove them.
class Node;
class NodeList;
class NodeRareDataBase;

template <
    typename T,
    bool mainThreadOnly =
        WTF::IsSubclass<typename std::remove_const<T>::type, Node>::value ||
        WTF::IsSubclass<typename std::remove_const<T>::type, NodeList>::value ||
        WTF::IsSubclass<typename std::remove_const<T>::type,
                        NodeRareDataBase>::value>
struct DefaultThreadingTrait;

template <typename T>
struct DefaultThreadingTrait<T, false> {
  STATIC_ONLY(DefaultThreadingTrait);
  static const ThreadAffinity kAffinity = kAnyThread;
};

template <typename T>
struct DefaultThreadingTrait<T, true> {
  STATIC_ONLY(DefaultThreadingTrait);
  static const ThreadAffinity kAffinity = kMainThreadOnly;
};

class HeapAllocator;
template <typename Table>
class HeapHashTableBacking;
template <typename T, typename Traits>
class HeapVectorBacking;
template <typename T>
class Member;
template <typename T>
class WeakMember;

template <typename T>
struct ThreadingTrait {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = DefaultThreadingTrait<T>::kAffinity;
};

template <typename U>
class ThreadingTrait<const U> : public ThreadingTrait<U> {};

template <typename T>
struct ThreadingTrait<Member<T>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T>
struct ThreadingTrait<WeakMember<T>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename Key, typename Value, typename T, typename U, typename V>
struct ThreadingTrait<HashMap<Key, Value, T, U, V, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity =
      (ThreadingTrait<Key>::kAffinity == kMainThreadOnly) &&
              (ThreadingTrait<Value>::kAffinity == kMainThreadOnly)
          ? kMainThreadOnly
          : kAnyThread;
};

template <typename First, typename Second>
struct ThreadingTrait<WTF::KeyValuePair<First, Second>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity =
      (ThreadingTrait<First>::kAffinity == kMainThreadOnly) &&
              (ThreadingTrait<Second>::kAffinity == kMainThreadOnly)
          ? kMainThreadOnly
          : kAnyThread;
};

template <typename T, typename U, typename V>
struct ThreadingTrait<HashSet<T, U, V, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T, size_t inlineCapacity>
struct ThreadingTrait<Vector<T, inlineCapacity, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T, typename Traits>
struct ThreadingTrait<HeapVectorBacking<T, Traits>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::Affinity;
};

template <typename T, size_t inlineCapacity>
struct ThreadingTrait<Deque<T, inlineCapacity, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename T, typename U, typename V>
struct ThreadingTrait<HashCountedSet<T, U, V, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
  static const ThreadAffinity kAffinity = ThreadingTrait<T>::kAffinity;
};

template <typename Table>
struct ThreadingTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(ThreadingTrait);
  using Key = typename Table::KeyType;
  using Value = typename Table::ValueType;
  static const ThreadAffinity kAffinity =
      (ThreadingTrait<Key>::Affinity == kMainThreadOnly) &&
              (ThreadingTrait<Value>::Affinity == kMainThreadOnly)
          ? kMainThreadOnly
          : kAnyThread;
};

template <typename T, typename U, typename V, typename W, typename X>
class HeapHashMap;
template <typename T, typename U, typename V>
class HeapHashSet;
template <typename T, wtf_size_t inlineCapacity>
class HeapVector;
template <typename T, wtf_size_t inlineCapacity>
class HeapDeque;
template <typename T, typename U, typename V>
class HeapHashCountedSet;

template <typename T, typename U, typename V, typename W, typename X>
struct ThreadingTrait<HeapHashMap<T, U, V, W, X>>
    : public ThreadingTrait<HashMap<T, U, V, W, X, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
};
template <typename T, typename U, typename V>
struct ThreadingTrait<HeapHashSet<T, U, V>>
    : public ThreadingTrait<HashSet<T, U, V, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
};
template <typename T, size_t inlineCapacity>
struct ThreadingTrait<HeapVector<T, inlineCapacity>>
    : public ThreadingTrait<Vector<T, inlineCapacity, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
};
template <typename T, size_t inlineCapacity>
struct ThreadingTrait<HeapDeque<T, inlineCapacity>>
    : public ThreadingTrait<Deque<T, inlineCapacity, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
};
template <typename T, typename U, typename V>
struct ThreadingTrait<HeapHashCountedSet<T, U, V>>
    : public ThreadingTrait<HashCountedSet<T, U, V, HeapAllocator>> {
  STATIC_ONLY(ThreadingTrait);
};

}  // namespace blink

#endif
