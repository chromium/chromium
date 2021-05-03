// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_

#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/buildflags.h"

#if BUILDFLAG(USE_V8_OILPAN)
#include "third_party/blink/renderer/platform/heap/v8_wrapper/collection_support/heap_hash_table_backing.h"
#else  // !USE_V8_OILPAN
#include "third_party/blink/renderer/platform/heap/impl/collection_support/heap_hash_table_backing.h"
#endif  // !USE_V8_OILPAN

namespace blink {

// Helper for processing ephemerons represented as KeyValuePair. Reorders
// parameters if needed so that KeyType is always weak.
template <typename _KeyType,
          typename _ValueType,
          typename _KeyTraits,
          typename _ValueTraits,
          bool = WTF::IsWeak<_ValueType>::value>
struct EphemeronKeyValuePair {
  using KeyType = _KeyType;
  using ValueType = _ValueType;
  using KeyTraits = _KeyTraits;
  using ValueTraits = _ValueTraits;

  // Ephemerons have different weakness for KeyType and ValueType. If weakness
  // is equal, we either have Strong/Strong, or Weak/Weak, which would indicate
  // a full strong or fully weak pair.
  static constexpr bool is_ephemeron =
      WTF::IsWeak<KeyType>::value != WTF::IsWeak<ValueType>::value;

  static_assert(!WTF::IsWeak<KeyType>::value ||
                    WTF::IsWeakMemberType<KeyType>::value,
                "Weakness must be encoded using WeakMember.");

  EphemeronKeyValuePair(const KeyType* k, const ValueType* v)
      : key(k), value(v) {}
  const KeyType* key;
  const ValueType* value;
};

template <typename _KeyType,
          typename _ValueType,
          typename _KeyTraits,
          typename _ValueTraits>
struct EphemeronKeyValuePair<_KeyType,
                             _ValueType,
                             _KeyTraits,
                             _ValueTraits,
                             true> : EphemeronKeyValuePair<_ValueType,
                                                           _KeyType,
                                                           _ValueTraits,
                                                           _KeyTraits,
                                                           false> {
  EphemeronKeyValuePair(const _KeyType* k, const _ValueType* v)
      : EphemeronKeyValuePair<_ValueType,
                              _KeyType,
                              _ValueTraits,
                              _KeyTraits,
                              false>(v, k) {}
};

}  // namespace blink

namespace WTF {

template <typename Table>
struct TraceInCollectionTrait<kNoWeakHandling,
                              blink::HeapHashTableBacking<Table>,
                              void> {
  static void Trace(blink::Visitor* visitor, const void* self) {
    TraceHashTableBackingInCollectionTrait<kNoWeakHandling, Table>::Trace(
        visitor, self);
  }
};

template <typename Table>
struct TraceInCollectionTrait<kWeakHandling,
                              blink::HeapHashTableBacking<Table>,
                              void> {
  static void Trace(blink::Visitor* visitor, const void* self) {
    TraceHashTableBackingInCollectionTrait<kWeakHandling, Table>::Trace(visitor,
                                                                        self);
  }
};

// This trace method is for tracing a HashTableBacking either through regular
// tracing (via the relevant TraceTraits) or when finding a HashTableBacking
// through conservative stack scanning (which will treat all references in the
// backing strongly).
template <WTF::WeakHandlingFlag WeakHandling,
          typename Key,
          typename Value,
          typename Traits>
struct TraceKeyValuePairInCollectionTrait {
  using EphemeronHelper =
      blink::EphemeronKeyValuePair<Key,
                                   Value,
                                   typename Traits::KeyTraits,
                                   typename Traits::ValueTraits>;

  static void Trace(blink::Visitor* visitor,
                    const Key* key,
                    const Value* value) {
    TraceImpl::Trace(visitor, key, value);
  }

 private:
  struct TraceImplEphemerons {
    // Strongification of ephemerons, i.e., Weak/Strong and Strong/Weak.
    static void Trace(blink::Visitor* visitor,
                      const Key* key,
                      const Value* value) {
      // Strongification of ephemerons, i.e., Weak/Strong and Strong/Weak.
      // The helper ensures that helper.key always refers to the weak part and
      // helper.value always refers to the dependent part.
      // We distinguish ephemeron from Weak/Weak and Strong/Strong to allow
      // users to override visitation behavior. An example is creating a heap
      // snapshot, where it is useful to annotate values as being kept alive
      // from keys rather than the table.
      EphemeronHelper helper(key, value);
      if (WeakHandling == kNoWeakHandling) {
        // Strongify the weak part.
        blink::TraceCollectionIfEnabled<
            kNoWeakHandling, typename EphemeronHelper::KeyType,
            typename EphemeronHelper::KeyTraits>::Trace(visitor, helper.key);
      }
      // The following passes on kNoWeakHandling for tracing value as the value
      // callback is only invoked to keep value alive iff key is alive,
      // following ephemeron semantics.
      visitor->TraceEphemeron(*helper.key, helper.value);
    }
  };

  struct TraceImplDefault {
    static void Trace(blink::Visitor* visitor,
                      const Key* key,
                      const Value* value) {
      // Strongification of non-ephemeron KVP, i.e., Strong/Strong or Weak/Weak.
      // Order does not matter here.
      blink::TraceCollectionIfEnabled<
          kNoWeakHandling, Key, typename Traits::KeyTraits>::Trace(visitor,
                                                                   key);
      blink::TraceCollectionIfEnabled<
          kNoWeakHandling, Value, typename Traits::ValueTraits>::Trace(visitor,
                                                                       value);
    }
  };

  using TraceImpl = typename std::conditional<
      EphemeronHelper::is_ephemeron &&
          WTF::IsTraceable<typename EphemeronHelper::ValueType>::value,
      TraceImplEphemerons,
      TraceImplDefault>::type;
};

// Trait for strong treatment of KeyValuePair. This is used to handle regular
// KVP but also for strongification of otherwise weakly handled KVPs.
template <typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling,
                              KeyValuePair<Key, Value>,
                              Traits> {
  static void Trace(blink::Visitor* visitor,
                    const KeyValuePair<Key, Value>& self) {
    TraceKeyValuePairInCollectionTrait<kNoWeakHandling, Key, Value,
                                       Traits>::Trace(visitor, &self.key,
                                                      &self.value);
  }
};

template <typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, KeyValuePair<Key, Value>, Traits> {
 private:
  template <typename T>
  struct NullReferenceChecker {
    static bool IsNull(const T& t) { return false; }
  };
  template <typename T>
  struct NullReferenceChecker<blink::Member<T>> {
    static bool IsNull(const blink::Member<T>& t) { return !t; }
  };
  template <typename T>
  struct NullReferenceChecker<blink::WeakMember<T>> {
    static bool IsNull(const blink::WeakMember<T>& t) { return !t; }
  };

 public:
  static bool IsAlive(const blink::LivenessBroker& info,
                      const KeyValuePair<Key, Value>& self) {
    // Needed for Weak/Weak, Strong/Weak (reverse ephemeron), and Weak/Strong
    // (ephemeron). Order of invocation does not matter as tracing weak key or
    // value does not have any side effects.
    //
    // Blink (reverse) ephemerons are allowed to temporarily contain a null key.
    // In case a GC happens before the key is overwritten with a non-null value,
    // IsAlive of weak KeyValuePair needs to consider null keys as alive (null
    // is generally treated as dead). Otherwise, weakness processing for the
    // hash table will delete the bucket even though it is still actively in
    // use. Since only the key of the (reverse) ephemeron can be null, pairs in
    // which both key and value are null do not need to be kept alive and can be
    // regarded as dead.
    bool key_is_null = NullReferenceChecker<Key>::IsNull(self.key);
    bool value_is_null = NullReferenceChecker<Value>::IsNull(self.value);
    return (key_is_null ||
            blink::TraceCollectionIfEnabled<
                WeakHandlingTrait<Key>::value, Key,
                typename Traits::KeyTraits>::IsAlive(info, self.key)) &&
           (value_is_null ||
            blink::TraceCollectionIfEnabled<
                WeakHandlingTrait<Value>::value, Value,
                typename Traits::ValueTraits>::IsAlive(info, self.value)) &&
           (!key_is_null || !value_is_null);
  }

  static void Trace(blink::Visitor* visitor,
                    const KeyValuePair<Key, Value>& self) {
    TraceKeyValuePairInCollectionTrait<kWeakHandling, Key, Value,
                                       Traits>::Trace(visitor, &self.key,
                                                      &self.value);
  }
};

template <typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<
    kNoWeakHandling,
    internal::ConcurrentBucket<KeyValuePair<Key, Value>>,
    Traits> {
  static void Trace(
      blink::Visitor* visitor,
      const internal::ConcurrentBucket<KeyValuePair<Key, Value>>& self) {
    TraceKeyValuePairInCollectionTrait<kNoWeakHandling, Key, Value,
                                       Traits>::Trace(visitor, self.key(),
                                                      self.value());
  }
};

template <typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<
    kWeakHandling,
    internal::ConcurrentBucket<KeyValuePair<Key, Value>>,
    Traits> {
  static void Trace(
      blink::Visitor* visitor,
      const internal::ConcurrentBucket<KeyValuePair<Key, Value>>& self) {
    TraceKeyValuePairInCollectionTrait<kWeakHandling, Key, Value,
                                       Traits>::Trace(visitor, self.key(),
                                                      self.value());
  }
};

template <typename T>
struct IsWeak<internal::ConcurrentBucket<T>> : IsWeak<T> {};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_
