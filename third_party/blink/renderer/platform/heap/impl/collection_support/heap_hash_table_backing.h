// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_

#include "third_party/blink/renderer/platform/heap/impl/heap_page.h"
#include "third_party/blink/renderer/platform/heap/impl/threading_traits.h"
#include "third_party/blink/renderer/platform/heap/impl/trace_traits.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/conditional_destructor.h"

namespace blink {

template <typename Table>
class HeapHashTableBacking final
    : public GarbageCollected<HeapHashTableBacking<Table>>,
      public WTF::ConditionalDestructor<
          HeapHashTableBacking<Table>,
          std::is_trivially_destructible<typename Table::ValueType>::value> {
 public:
  template <typename Backing>
  static void* AllocateObject(size_t);

  // Conditionally invoked via destructor.
  void Finalize();
};

template <typename Table>
struct ThreadingTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(ThreadingTrait);
  using Key = typename Table::KeyType;
  using Value = typename Table::ValueType;
  static const ThreadAffinity kAffinity =
      (ThreadingTrait<Key>::kAffinity == kMainThreadOnly) &&
              (ThreadingTrait<Value>::kAffinity == kMainThreadOnly)
          ? kMainThreadOnly
          : kAnyThread;
};

// static
template <typename Table>
template <typename Backing>
void* HeapHashTableBacking<Table>::AllocateObject(size_t size) {
  ThreadState* state =
      ThreadStateFor<ThreadingTrait<Backing>::kAffinity>::GetState();
  DCHECK(state->IsAllocationAllowed());
  return state->Heap().AllocateOnArenaIndex(
      state, size, BlinkGC::kHashTableArenaIndex, GCInfoTrait<Backing>::Index(),
      WTF_HEAP_PROFILER_TYPE_NAME(Backing));
}

template <typename Table>
void HeapHashTableBacking<Table>::Finalize() {
  using Value = typename Table::ValueType;
  static_assert(
      !std::is_trivially_destructible<Value>::value,
      "Finalization of trivially destructible classes should not happen.");
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(this);
  // Use the payload size as recorded by the heap to determine how many
  // elements to finalize.
  size_t length = header->PayloadSize() / sizeof(Value);
  Value* table = reinterpret_cast<Value*>(this);
  for (unsigned i = 0; i < length; ++i) {
    if (!Table::IsEmptyOrDeletedBucket(table[i]))
      table[i].~Value();
  }
}

template <typename Table>
struct MakeGarbageCollectedTrait<HeapHashTableBacking<Table>> {
  static HeapHashTableBacking<Table>* Call(size_t num_elements) {
    static_assert(!std::is_polymorphic<HeapHashTableBacking<Table>>::value,
                  "HeapHashTableBacking must not be polymorphic as it is "
                  "converted to a raw array of buckets for certain operation");
    CHECK_GT(num_elements, 0u);
    void* memory = HeapHashTableBacking<Table>::template AllocateObject<
        HeapHashTableBacking<Table>>(num_elements *
                                     sizeof(typename Table::ValueType));
    HeapObjectHeader* header = HeapObjectHeader::FromPayload(memory);
    // Placement new as regular operator new() is deleted.
    HeapHashTableBacking<Table>* object =
        ::new (memory) HeapHashTableBacking<Table>();
    header->MarkFullyConstructed<HeapObjectHeader::AccessMode::kAtomic>();
    return object;
  }
};

// The trace trait for the heap hashtable backing is used when we find a
// direct pointer to the backing from the conservative stack scanner. This
// normally indicates that there is an ongoing iteration over the table, and so
// we disable weak processing of table entries. When the backing is found
// through the owning hash table we mark differently, in order to do weak
// processing.
template <typename Table>
struct TraceTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(TraceTrait);
  using Backing = HeapHashTableBacking<Table>;
  using ValueType = typename Table::ValueTraits::TraitType;
  using Traits = typename Table::ValueTraits;

 public:
  static TraceDescriptor GetTraceDescriptor(const void* self) {
    return {self, Trace<WTF::kNoWeakHandling>};
  }

  static TraceDescriptor GetWeakTraceDescriptor(const void* self) {
    return GetWeakTraceDescriptorImpl<ValueType>::GetWeakTraceDescriptor(self);
  }

  template <WTF::WeakHandlingFlag WeakHandling = WTF::kNoWeakHandling>
  static void Trace(Visitor* visitor, const void* self) {
    if (!Traits::kCanTraceConcurrently && self) {
      if (visitor->DeferredTraceIfConcurrent({self, &Trace<WeakHandling>},
                                             GetBackingStoreSize(self)))
        return;
    }

    static_assert(WTF::IsTraceableInCollectionTrait<Traits>::value ||
                      WTF::IsWeak<ValueType>::value,
                  "T should not be traced");
    WTF::TraceInCollectionTrait<WeakHandling, Backing, void>::Trace(visitor,
                                                                    self);
  }

 private:
  static size_t GetBackingStoreSize(const void* backing_store) {
    const HeapObjectHeader* header =
        HeapObjectHeader::FromPayload(backing_store);
    return header->IsLargeObject<HeapObjectHeader::AccessMode::kAtomic>()
               ? static_cast<LargeObjectPage*>(PageFromObject(header))
                     ->ObjectSize()
               : header->size<HeapObjectHeader::AccessMode::kAtomic>();
  }

  template <typename ValueType>
  struct GetWeakTraceDescriptorImpl {
    static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
      return {backing, nullptr};
    }
  };

  template <typename K, typename V>
  struct GetWeakTraceDescriptorImpl<WTF::KeyValuePair<K, V>> {
    static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
      return GetWeakTraceDescriptorKVPImpl<K, V>::GetWeakTraceDescriptor(
          backing);
    }

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

    template <typename KeyType, typename ValueType>
    struct GetWeakTraceDescriptorKVPImpl<KeyType, ValueType, true> {
      static TraceDescriptor GetWeakTraceDescriptor(const void* backing) {
        return {backing, Trace<WTF::kWeakHandling>};
      }
    };
  };
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

// This trace method is for tracing a HashTableBacking either through regular
// tracing (via the relevant TraceTraits) or when finding a HashTableBacking
// through conservative stack scanning (which will treat all references in the
// backing strongly).
template <WTF::WeakHandlingFlag WeakHandling, typename Table>
struct TraceHashTableBackingInCollectionTrait {
  using Value = typename Table::ValueType;
  using Traits = typename Table::ValueTraits;
  using Extractor = typename Table::ExtractorType;

  static void Trace(blink::Visitor* visitor, const void* self) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value ||
                      WTF::IsWeak<Value>::value,
                  "Table should not be traced");
    const Value* array = reinterpret_cast<const Value*>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::FromPayload(self);
    // Use the payload size as recorded by the heap to determine how many
    // elements to trace.
    size_t length = header->PayloadSize() / sizeof(Value);
    const bool is_concurrent = visitor->IsConcurrent();
    for (size_t i = 0; i < length; ++i) {
      // If tracing concurrently, use a concurrent-safe version of
      // IsEmptyOrDeletedBucket (check performed on a local copy instead
      // of directly on the bucket).
      if (is_concurrent) {
        internal::ConcurrentBucket<Value> concurrent_bucket(
            array[i], Extractor::ExtractSafe);
        if (!HashTableHelper<Value, Extractor, typename Table::KeyTraitsType>::
                IsEmptyOrDeletedBucketForKey(*concurrent_bucket.key())) {
          blink::TraceCollectionIfEnabled<
              WeakHandling,
              typename internal::ConcurrentBucket<Value>::BucketType,
              Traits>::Trace(visitor, concurrent_bucket.bucket());
        }
      } else {
        if (!HashTableHelper<Value, Extractor, typename Table::KeyTraitsType>::
                IsEmptyOrDeletedBucket(array[i])) {
          blink::TraceCollectionIfEnabled<WeakHandling, Value, Traits>::Trace(
              visitor, &array[i]);
        }
      }
    }
  }
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_IMPL_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_
