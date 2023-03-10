// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_

#include <type_traits>

#include "base/check_op.h"
#include "third_party/blink/renderer/platform/heap/custom_spaces.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state_storage.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/conditional_destructor.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"
#include "third_party/blink/renderer/platform/wtf/key_value_pair.h"
#include "third_party/blink/renderer/platform/wtf/sanitizers.h"
#include "v8/include/cppgc/custom-space.h"
#include "v8/include/cppgc/explicit-management.h"
#include "v8/include/cppgc/object-size-trait.h"

namespace blink {

template <typename Table>
class HeapHashTableBacking final
    : public GarbageCollected<HeapHashTableBacking<Table>>,
      public WTF::ConditionalDestructor<
          HeapHashTableBacking<Table>,
          !std::is_trivially_destructible<typename Table::ValueType>::value> {
  using ClassType = HeapHashTableBacking<Table>;
  using ValueType = typename Table::ValueType;

 public:
  // Although the HeapHashTableBacking is fully constructed, the array resulting
  // from ToArray may not be fully constructed as the elements of the array are
  // not initialized and may have null vtable pointers. Null vtable pointer
  // violates CFI for polymorphic types.
  ALWAYS_INLINE NO_SANITIZE_UNRELATED_CAST static ValueType* ToArray(
      ClassType* backing) {
    return reinterpret_cast<ValueType*>(backing);
  }

  ALWAYS_INLINE static ClassType* FromArray(ValueType* array) {
    return reinterpret_cast<ClassType*>(array);
  }

  void Free(cppgc::HeapHandle& heap_handle) {
    cppgc::subtle::FreeUnreferencedObject(heap_handle, *this);
  }

  bool Resize(size_t new_size) {
    return cppgc::subtle::Resize(*this, GetAdditionalBytes(new_size));
  }

  // Conditionally invoked via destructor.
  void Finalize();

 private:
  static cppgc::AdditionalBytes GetAdditionalBytes(size_t wanted_array_size) {
    // HHTB is an empty class that's purely used with inline storage. Since its
    // sizeof(HHTB) == 1, we need to subtract its size to avoid wasting storage.
    static_assert(sizeof(ClassType) == 1, "Class declaration changed");
    DCHECK_GE(wanted_array_size, sizeof(ClassType));
    return cppgc::AdditionalBytes{wanted_array_size - sizeof(ClassType)};
  }
};

template <typename Table>
void HeapHashTableBacking<Table>::Finalize() {
  using Value = typename Table::ValueType;
  static_assert(
      !std::is_trivially_destructible<Value>::value,
      "Finalization of trivially destructible classes should not happen.");
  const size_t object_size =
      cppgc::subtle::ObjectSizeTrait<HeapHashTableBacking<Table>>::GetSize(
          *this);
  const size_t length = object_size / sizeof(Value);
  Value* table = reinterpret_cast<Value*>(this);
  for (unsigned i = 0; i < length; ++i) {
    if (!Table::IsEmptyOrDeletedBucket(table[i]))
      table[i].~Value();
  }
}

template <typename Table>
struct ThreadingTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(ThreadingTrait);
  using Key = typename Table::KeyType;
  using Value = typename Table::ValueType;
  static constexpr ThreadAffinity kAffinity =
      (ThreadingTrait<Key>::kAffinity == kMainThreadOnly) &&
              (ThreadingTrait<Value>::kAffinity == kMainThreadOnly)
          ? kMainThreadOnly
          : kAnyThread;
};

template <typename First, typename Second>
struct ThreadingTrait<WTF::KeyValuePair<First, Second>> {
  STATIC_ONLY(ThreadingTrait);
  static constexpr ThreadAffinity kAffinity =
      (ThreadingTrait<First>::kAffinity == kMainThreadOnly) &&
              (ThreadingTrait<Second>::kAffinity == kMainThreadOnly)
          ? kMainThreadOnly
          : kAnyThread;
};

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

namespace internal {

// ConcurrentBucket is a wrapper for HashTable buckets for concurrent marking.
// It is used to provide a snapshot view of the bucket key and guarantee
// that the same key is used for checking empty/deleted buckets and tracing.
template <typename T>
class ConcurrentBucket {
  using KeyExtractionCallback = void (*)(const T&, void*);

 public:
  using BucketType = T;

  ConcurrentBucket(const T& t, KeyExtractionCallback extract_key) {
    extract_key(t, &buf_);
  }

  // for HashTable that don't use KeyValuePair (i.e. *HashSets), the key
  // and the value are the same.
  const T* key() const { return reinterpret_cast<const T*>(&buf_); }
  const T* value() const { return key(); }
  const T* bucket() const { return key(); }

 private:
  // Alignment is needed for atomic accesses to |buf_| and to assure |buf_|
  // can be accessed the same as objects of type T
  static constexpr size_t boundary = std::max(alignof(T), sizeof(size_t));
  alignas(boundary) char buf_[sizeof(T)];
};

template <typename Key, typename Value>
class ConcurrentBucket<KeyValuePair<Key, Value>> {
  using KeyExtractionCallback = void (*)(const KeyValuePair<Key, Value>&,
                                         void*);

 public:
  using BucketType = ConcurrentBucket;

  ConcurrentBucket(const KeyValuePair<Key, Value>& pair,
                   KeyExtractionCallback extract_key)
      : value_(&pair.value) {
    extract_key(pair, &buf_);
  }

  const Key* key() const { return reinterpret_cast<const Key*>(&buf_); }
  const Value* value() const { return value_; }
  const ConcurrentBucket* bucket() const { return this; }

 private:
  // Alignment is needed for atomic accesses to |buf_| and to assure |buf_|
  // can be accessed the same as objects of type Key
  static constexpr size_t boundary = std::max(alignof(Key), sizeof(size_t));
  alignas(boundary) char buf_[sizeof(Key)];
  const Value* value_;
};

}  // namespace internal

template <WTF::WeakHandlingFlag weak_handling, typename Table>
struct TraceHashTableBackingInCollectionTrait {
  using Value = typename Table::ValueType;
  using Traits = typename Table::ValueTraits;
  using Extractor = typename Table::ExtractorType;

  static void Trace(blink::Visitor* visitor, const void* self) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value ||
                      WTF::IsWeak<Value>::value,
                  "Table should not be traced");
    const Value* array = reinterpret_cast<const Value*>(self);
    const size_t length =
        cppgc::subtle::
            ObjectSizeTrait<const blink::HeapHashTableBacking<Table>>::GetSize(
                *reinterpret_cast<const blink::HeapHashTableBacking<Table>*>(
                    self)) /
        sizeof(Value);
    for (size_t i = 0; i < length; ++i) {
      internal::ConcurrentBucket<Value> concurrent_bucket(
          array[i], Extractor::ExtractKeyToMemory);
      if (!WTF::IsHashTraitsEmptyOrDeletedValue<typename Table::KeyTraitsType>(
              *concurrent_bucket.key())) {
        blink::TraceCollectionIfEnabled<
            weak_handling,
            typename internal::ConcurrentBucket<Value>::BucketType,
            Traits>::Trace(visitor, concurrent_bucket.bucket());
      }
    }
  }
};

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
  static bool IsAlive(const blink::LivenessBroker& info,
                      const KeyValuePair<Key, Value>& self) {
    // Needed for Weak/Weak, Strong/Weak (reverse ephemeron), and Weak/Strong
    // (ephemeron). Order of invocation does not matter as tracing weak key or
    // value does not have any side effects.
    return blink::TraceCollectionIfEnabled<
               WeakHandlingTrait<Key>::value, Key,
               typename Traits::KeyTraits>::IsAlive(info, self.key) &&
           blink::TraceCollectionIfEnabled<
               WeakHandlingTrait<Value>::value, Value,
               typename Traits::ValueTraits>::IsAlive(info, self.value);
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

namespace cppgc {

// Assign HeapVector to the custom HeapVectorBackingSpace.
template <typename Table>
struct SpaceTrait<blink::HeapHashTableBacking<Table>> {
  using Space = blink::HeapHashTableBackingSpace;
};

// Custom allocation accounts for inlined storage of the actual elements of the
// backing table.
template <typename Table>
class MakeGarbageCollectedTrait<blink::HeapHashTableBacking<Table>>
    : public MakeGarbageCollectedTraitBase<blink::HeapHashTableBacking<Table>> {
 public:
  using Backing = blink::HeapHashTableBacking<Table>;

  template <typename... Args>
  static Backing* Call(AllocationHandle& handle, size_t num_elements) {
    static_assert(
        !std::is_polymorphic<blink::HeapHashTableBacking<Table>>::value,
        "HeapHashTableBacking must not be polymorphic as it is converted to a "
        "raw array of buckets for certain operation");
    CHECK_GT(num_elements, 0u);
    // Allocate automatically considers the custom space via SpaceTrait.
    void* memory = MakeGarbageCollectedTraitBase<Backing>::Allocate(
        handle, sizeof(typename Table::ValueType) * num_elements);
    Backing* object = ::new (memory) Backing();
    MakeGarbageCollectedTraitBase<Backing>::MarkObjectAsFullyConstructed(
        object);
    return object;
  }
};

template <typename Table>
struct TraceTrait<blink::HeapHashTableBacking<Table>> {
  using Backing = blink::HeapHashTableBacking<Table>;
  using Traits = typename Table::ValueTraits;
  using ValueType = typename Table::ValueTraits::TraitType;

  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return {self, Trace<WTF::kNoWeakHandling>};
  }

  static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
    return GetWeakTraceDescriptorImpl<ValueType>::GetWeakTraceDescriptor(self);
  }

  template <WTF::WeakHandlingFlag weak_handling = WTF::kNoWeakHandling>
  static void Trace(Visitor* visitor, const void* self) {
    if (!Traits::kCanTraceConcurrently && self) {
      if (visitor->DeferTraceToMutatorThreadIfConcurrent(
              self, &Trace<weak_handling>,
              cppgc::subtle::ObjectSizeTrait<const Backing>::GetSize(
                  *reinterpret_cast<const Backing*>(self)))) {
        return;
      }
    }

    static_assert(WTF::IsTraceableInCollectionTrait<Traits>::value ||
                      WTF::IsWeak<ValueType>::value,
                  "T should not be traced");
    WTF::TraceInCollectionTrait<weak_handling, Backing, void>::Trace(visitor,
                                                                     self);
  }

 private:
  // Default setting for HashTable is without weak trace descriptor.
  template <typename ValueType>
  struct GetWeakTraceDescriptorImpl {
    static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
      return {self, nullptr};
    }
  };

  // Specialization for WTF::KeyValuePair, which is default bucket storage type.
  template <typename K, typename V>
  struct GetWeakTraceDescriptorImpl<WTF::KeyValuePair<K, V>> {
    static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
      return GetWeakTraceDescriptorKVPImpl<K, V>::GetWeakTraceDescriptor(
          backing);
    }

    // Default setting for KVP without ephemeron semantics.
    template <typename KeyType,
              typename ValueType,
              bool ephemeron_semantics = (WTF::IsWeak<KeyType>::value &&
                                          !WTF::IsWeak<ValueType>::value &&
                                          WTF::IsTraceable<ValueType>::value) ||
                                         (WTF::IsWeak<ValueType>::value &&
                                          !WTF::IsWeak<KeyType>::value &&
                                          WTF::IsTraceable<KeyType>::value)>
    struct GetWeakTraceDescriptorKVPImpl {
      static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
        return {backing, nullptr};
      }
    };

    // Specialization for KVP with ephemeron semantics.
    template <typename KeyType, typename ValueType>
    struct GetWeakTraceDescriptorKVPImpl<KeyType, ValueType, true> {
      static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
        return {backing, Trace<WTF::kWeakHandling>};
      }
    };
  };
};

}  // namespace cppgc

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_
