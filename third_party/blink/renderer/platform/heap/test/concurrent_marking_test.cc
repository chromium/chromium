// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(THREAD_SANITIZER)

#include "base/compiler_specific.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_counted_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_objects.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"

namespace blink {

class ConcurrentMarkingTest : public TestSupportingGC {};

namespace concurrent_marking_test {

template <typename T>
class CollectionWrapper : public GarbageCollected<CollectionWrapper<T>> {
 public:
  CollectionWrapper() : collection_(MakeGarbageCollected<T>()) {}

  void Trace(Visitor* visitor) const { visitor->Trace(collection_); }

  T* GetCollection() { return collection_.Get(); }

 private:
  Member<T> collection_;
};

// =============================================================================
// Tests that expose data races when modifying collections =====================
// =============================================================================

template <typename T>
struct MethodAdapterBase {
  template <typename U>
  static void insert(T& t, U&& u) {
    t.insert(std::forward<U>(u));
  }

  static void erase(T& t, typename T::iterator&& it) {
    t.erase(std::forward<typename T::iterator>(it));
  }

  static void Swap(T& a, T& b) { a.swap(b); }
};

template <typename T>
struct MethodAdapter : public MethodAdapterBase<T> {};

template <typename C>
void AddToCollection() {
  constexpr int kIterations = 10;
  ConcurrentMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  driver.StartGC();
  for (int i = 0; i < kIterations; ++i) {
    driver.TriggerMarkingSteps();
    for (int j = 0; j < kIterations; ++j) {
      int num = kIterations * i + j;
      MethodAdapter<C>::insert(*collection,
                               MakeGarbageCollected<IntegerObject>(num));
    }
  }
  driver.FinishGC();
}

template <typename C, typename GetLocation>
void RemoveFromCollectionAtLocation(GetLocation location) {
  constexpr int kIterations = 10;
  ConcurrentMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  for (int i = 0; i < (kIterations * kIterations); ++i) {
    MethodAdapter<C>::insert(*collection,
                             MakeGarbageCollected<IntegerObject>(i));
  }
  driver.StartGC();
  for (int i = 0; i < kIterations; ++i) {
    driver.TriggerMarkingSteps();
    for (int j = 0; j < kIterations; ++j) {
      MethodAdapter<C>::erase(*collection, location(collection));
    }
  }
  driver.FinishGC();
}

template <typename C>
void RemoveFromBeginningOfCollection() {
  RemoveFromCollectionAtLocation<C>(
      [](C* collection) { return collection->begin(); });
}

template <typename C>
void RemoveFromMiddleOfCollection() {
  RemoveFromCollectionAtLocation<C>([](C* collection) {
    auto iterator = collection->begin();
    // Move iterator to middle of collection.
    for (size_t i = 0; i < collection->size() / 2; ++i) {
      UNSAFE_TODO(++iterator);
    }
    return iterator;
  });
}

template <typename C>
void RemoveFromEndOfCollection() {
  RemoveFromCollectionAtLocation<C>([](C* collection) {
    auto iterator = collection->end();
    return UNSAFE_TODO(--iterator);
  });
}

template <typename C>
void ClearCollection() {
  constexpr int kIterations = 10;
  ConcurrentMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  driver.StartGC();
  for (int i = 0; i < kIterations; ++i) {
    driver.TriggerMarkingSteps();
    for (int j = 0; j < kIterations; ++j) {
      MethodAdapter<C>::insert(*collection,
                               MakeGarbageCollected<IntegerObject>(i));
    }
    collection->clear();
  }
  driver.FinishGC();
}

template <typename C>
void SwapCollections() {
  constexpr int kIterations = 10;
  ConcurrentMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  driver.StartGC();
  for (int i = 0; i < (kIterations * kIterations); ++i) {
    C* new_collection = MakeGarbageCollected<C>();
    for (int j = 0; j < kIterations * i; ++j) {
      MethodAdapter<C>::insert(*new_collection,
                               MakeGarbageCollected<IntegerObject>(j));
    }
    driver.TriggerMarkingSteps();
    MethodAdapter<C>::Swap(*collection, *new_collection);
  }
  driver.FinishGC();
}

// HeapHashMap

template <typename T>
using IdentityHashMap = GCedHeapHashMap<T, T>;

template <typename T>
struct MethodAdapter<GCedHeapHashMap<T, T>>
    : public MethodAdapterBase<GCedHeapHashMap<T, T>> {
  template <typename U>
  static void insert(GCedHeapHashMap<T, T>& map, U&& u) {
    map.insert(u, u);
  }
};

TEST_F(ConcurrentMarkingTest, AddToHashMap) {
  AddToCollection<IdentityHashMap<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfHashMap) {
  RemoveFromBeginningOfCollection<IdentityHashMap<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfHashMap) {
  RemoveFromMiddleOfCollection<IdentityHashMap<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfHashMap) {
  RemoveFromEndOfCollection<IdentityHashMap<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearHashMap) {
  ClearCollection<IdentityHashMap<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapHashMap) {
  SwapCollections<IdentityHashMap<Member<IntegerObject>>>();
}

// HeapHashSet

TEST_F(ConcurrentMarkingTest, AddToHashSet) {
  AddToCollection<GCedHeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfHashSet) {
  RemoveFromBeginningOfCollection<GCedHeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfHashSet) {
  RemoveFromMiddleOfCollection<GCedHeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfHashSet) {
  RemoveFromEndOfCollection<GCedHeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearHashSet) {
  ClearCollection<GCedHeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapHashSet) {
  SwapCollections<GCedHeapHashSet<Member<IntegerObject>>>();
}

template <typename T>
struct MethodAdapter<GCedHeapLinkedHashSet<T>>
    : public MethodAdapterBase<GCedHeapLinkedHashSet<T>> {
  static void Swap(GCedHeapLinkedHashSet<T>& a, GCedHeapLinkedHashSet<T>& b) {
    a.Swap(b);
  }
};

TEST_F(ConcurrentMarkingTest, AddToLinkedHashSet) {
  AddToCollection<GCedHeapLinkedHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfLinkedHashSet) {
  RemoveFromBeginningOfCollection<
      GCedHeapLinkedHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfLinkedHashSet) {
  RemoveFromMiddleOfCollection<GCedHeapLinkedHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfLinkedHashSet) {
  RemoveFromEndOfCollection<GCedHeapLinkedHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearLinkedHashSet) {
  ClearCollection<GCedHeapLinkedHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapLinkedHashSet) {
  SwapCollections<GCedHeapLinkedHashSet<Member<IntegerObject>>>();
}

// HeapHashCountedSet

TEST_F(ConcurrentMarkingTest, AddToHashCountedSet) {
  AddToCollection<GCedHeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfHashCountedSet) {
  RemoveFromBeginningOfCollection<
      GCedHeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfHashCountedSet) {
  RemoveFromMiddleOfCollection<GCedHeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfHashCountedSet) {
  RemoveFromEndOfCollection<GCedHeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearHashCountedSet) {
  ClearCollection<GCedHeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapHashCountedSet) {
  SwapCollections<GCedHeapHashCountedSet<Member<IntegerObject>>>();
}

// HeapVector

// Additional test for vectors and deques
template <typename V>
void PopFromCollection() {
  constexpr int kIterations = 10;
  ConcurrentMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<V>> persistent =
      MakeGarbageCollected<CollectionWrapper<V>>();
  V* vector = persistent->GetCollection();
  for (int i = 0; i < (kIterations * kIterations); ++i) {
    MethodAdapter<V>::insert(*vector, MakeGarbageCollected<IntegerObject>(i));
  }
  driver.StartGC();
  for (int i = 0; i < kIterations; ++i) {
    driver.TriggerMarkingSteps();
    for (int j = 0; j < kIterations; ++j) {
      vector->pop_back();
    }
  }
  driver.FinishGC();
}

template <typename T, wtf_size_t inlineCapacity>
struct MethodAdapter<GCedHeapVector<T, inlineCapacity>>
    : public MethodAdapterBase<GCedHeapVector<T, inlineCapacity>> {
  template <typename U>
  static void insert(GCedHeapVector<T, inlineCapacity>& vector, U&& u) {
    vector.push_back(std::forward<U>(u));
  }
};

TEST_F(ConcurrentMarkingTest, AddToVector) {
  AddToCollection<GCedHeapVector<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfVector) {
  RemoveFromBeginningOfCollection<GCedHeapVector<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfVector) {
  RemoveFromMiddleOfCollection<GCedHeapVector<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfVector) {
  RemoveFromEndOfCollection<GCedHeapVector<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearVector) {
  ClearCollection<GCedHeapVector<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapVector) {
  SwapCollections<GCedHeapVector<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, PopFromVector) {
  PopFromCollection<GCedHeapVector<Member<IntegerObject>>>();
}

// HeapVector with inlined buffer

template <typename T>
using HeapVectorWithInlineStorage = GCedHeapVector<T, 10>;

TEST_F(ConcurrentMarkingTest, AddToInlinedVector) {
  AddToCollection<HeapVectorWithInlineStorage<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfInlinedVector) {
  RemoveFromBeginningOfCollection<
      HeapVectorWithInlineStorage<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfInlinedVector) {
  RemoveFromMiddleOfCollection<
      HeapVectorWithInlineStorage<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfInlinedVector) {
  RemoveFromEndOfCollection<
      HeapVectorWithInlineStorage<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearInlinedVector) {
  ClearCollection<HeapVectorWithInlineStorage<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapInlinedVector) {
  SwapCollections<HeapVectorWithInlineStorage<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, PopFromInlinedVector) {
  PopFromCollection<HeapVectorWithInlineStorage<Member<IntegerObject>>>();
}

// HeapVector of std::pairs

template <typename T>
using HeapVectorOfPairs = GCedHeapVector<std::pair<T, T>>;

template <typename T, wtf_size_t inlineCapacity>
struct MethodAdapter<GCedHeapVector<std::pair<T, T>, inlineCapacity>>
    : public MethodAdapterBase<
          GCedHeapVector<std::pair<T, T>, inlineCapacity>> {
  template <typename U>
  static void insert(GCedHeapVector<std::pair<T, T>, inlineCapacity>& vector,
                     U&& u) {
    vector.push_back(std::make_pair<U&, U&>(u, u));
  }
};

TEST_F(ConcurrentMarkingTest, AddToVectorOfPairs) {
  AddToCollection<HeapVectorOfPairs<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfVectorOfPairs) {
  RemoveFromBeginningOfCollection<HeapVectorOfPairs<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfVectorOfPairs) {
  RemoveFromMiddleOfCollection<HeapVectorOfPairs<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfVectorOfPairs) {
  RemoveFromEndOfCollection<HeapVectorOfPairs<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearVectorOfPairs) {
  ClearCollection<HeapVectorOfPairs<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapVectorOfPairs) {
  SwapCollections<HeapVectorOfPairs<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, PopFromVectorOfPairs) {
  PopFromCollection<HeapVectorOfPairs<Member<IntegerObject>>>();
}

// HeapDeque

template <typename T>
struct MethodAdapter<GCedHeapDeque<T>>
    : public MethodAdapterBase<GCedHeapDeque<T>> {
  template <typename U>
  static void insert(GCedHeapDeque<T>& deque, U&& u) {
    deque.push_back(std::forward<U>(u));
  }

  static void erase(GCedHeapDeque<T>& deque,
                    typename GCedHeapDeque<T>::iterator&& it) {
    deque.pop_back();
  }

  static void Swap(GCedHeapDeque<T>& a, GCedHeapDeque<T>& b) { a.Swap(b); }
};

TEST_F(ConcurrentMarkingTest, AddToDeque) {
  AddToCollection<GCedHeapDeque<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfDeque) {
  RemoveFromBeginningOfCollection<GCedHeapDeque<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfDeque) {
  RemoveFromMiddleOfCollection<GCedHeapDeque<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfDeque) {
  RemoveFromEndOfCollection<GCedHeapDeque<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearDeque) {
  ClearCollection<GCedHeapDeque<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapDeque) {
  SwapCollections<GCedHeapDeque<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, PopFromDeque) {
  PopFromCollection<GCedHeapDeque<Member<IntegerObject>>>();
}

namespace {

class RegisteredMixin;

class CollectsMixins : public GarbageCollected<CollectsMixins> {
  using MixinSet = GCedHeapHashSet<Member<RegisteredMixin>>;

 public:
  CollectsMixins() : set_(MakeGarbageCollected<MixinSet>()) {}
  void RegisterMixin(RegisteredMixin* mixin) { set_->insert(mixin); }
  void Trace(Visitor* visitor) const { visitor->Trace(set_); }

 private:
  Member<MixinSet> set_;
};

class RegisteredMixin : public GarbageCollectedMixin {
 public:
  RegisteredMixin(CollectsMixins* collector) { collector->RegisterMixin(this); }
};

class GCedWithRegisteredMixin
    : public GarbageCollected<GCedWithRegisteredMixin>,
      public RegisteredMixin {
 public:
  GCedWithRegisteredMixin(CollectsMixins* collector)
      : RegisteredMixin(collector) {}
  void Trace(Visitor*) const override {}
};

}  // namespace

TEST_F(ConcurrentMarkingTest, MarkingInConstructionMixin) {
  constexpr int kIterations = 10;
  ConcurrentMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectsMixins> collector = MakeGarbageCollected<CollectsMixins>();
  driver.StartGC();
  for (int i = 0; i < kIterations; ++i) {
    driver.TriggerMarkingSteps();
    for (int j = 0; j < kIterations; ++j) {
      MakeGarbageCollected<GCedWithRegisteredMixin>(collector.Get());
    }
  }
  driver.FinishGC();
}

}  // namespace concurrent_marking_test
}  // namespace blink

#endif  // defined(THREAD_SANITIZER)
