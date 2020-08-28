// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(THREAD_SANITIZER)

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
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

template <typename C>
void AddToCollection() {
  constexpr int kIterations = 10;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < kIterations; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < kIterations; ++j) {
      int num = kIterations * i + j;
      collection->insert(MakeGarbageCollected<IntegerObject>(num));
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

template <typename C, typename GetLocation>
void RemoveFromCollectionAtLocation(GetLocation location) {
  constexpr int kIterations = 10;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  for (int i = 0; i < (kIterations * kIterations); ++i) {
    collection->insert(MakeGarbageCollected<IntegerObject>(i));
  }
  driver.Start();
  for (int i = 0; i < kIterations; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < kIterations; ++j) {
      collection->erase(location(collection));
    }
  }
  driver.FinishSteps();
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
      ++iterator;
    }
    return iterator;
  });
}

template <typename C>
void RemoveFromEndOfCollection() {
  RemoveFromCollectionAtLocation<C>([](C* collection) {
    auto iterator = collection->end();
    return --iterator;
  });
}

template <typename C>
void ClearCollection() {
  constexpr int kIterations = 10;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < kIterations; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < kIterations; ++j) {
      collection->insert(MakeGarbageCollected<IntegerObject>(i));
    }
    collection->clear();
  }
  driver.FinishSteps();
  driver.FinishGC();
}

template <typename C>
void SwapCollections() {
  constexpr int kIterations = 10;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<C>> persistent =
      MakeGarbageCollected<CollectionWrapper<C>>();
  C* collection = persistent->GetCollection();
  driver.Start();
  for (int i = 0; i < (kIterations * kIterations); ++i) {
    C* new_collection = MakeGarbageCollected<C>();
    for (int j = 0; j < kIterations * i; ++j) {
      new_collection->insert(MakeGarbageCollected<IntegerObject>(j));
    }
    driver.SingleConcurrentStep();
    collection->swap(*new_collection);
  }
  driver.FinishSteps();
  driver.FinishGC();
}

// HeapHashMap

template <typename T>
class HeapHashMapAdapter : public HeapHashMap<T, T> {
 public:
  template <typename U>
  ALWAYS_INLINE void insert(U* u) {
    HeapHashMap<T, T>::insert(u, u);
  }
};

TEST_F(ConcurrentMarkingTest, AddToHashMap) {
  AddToCollection<HeapHashMapAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfHashMap) {
  RemoveFromBeginningOfCollection<HeapHashMapAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfHashMap) {
  RemoveFromMiddleOfCollection<HeapHashMapAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfHashMap) {
  RemoveFromEndOfCollection<HeapHashMapAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearHashMap) {
  ClearCollection<HeapHashMapAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapHashMap) {
  SwapCollections<HeapHashMapAdapter<Member<IntegerObject>>>();
}

// HeapHashSet

TEST_F(ConcurrentMarkingTest, AddToHashSet) {
  AddToCollection<HeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfHashSet) {
  RemoveFromBeginningOfCollection<HeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfHashSet) {
  RemoveFromMiddleOfCollection<HeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfHashSet) {
  RemoveFromEndOfCollection<HeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearHashSet) {
  ClearCollection<HeapHashSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapHashSet) {
  SwapCollections<HeapHashSet<Member<IntegerObject>>>();
}

// HeapLinkedHashSet
template <typename T>
class HeapLinkedHashSetAdapter : public HeapLinkedHashSet<T> {
 public:
  ALWAYS_INLINE void swap(HeapLinkedHashSetAdapter<T>& other) {
    HeapLinkedHashSet<T>::Swap(other);
  }
};

TEST_F(ConcurrentMarkingTest, AddToLinkedHashSet) {
  AddToCollection<HeapLinkedHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfLinkedHashSet) {
  RemoveFromBeginningOfCollection<
      HeapLinkedHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfLinkedHashSet) {
  RemoveFromMiddleOfCollection<
      HeapLinkedHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfLinkedHashSet) {
  RemoveFromEndOfCollection<HeapLinkedHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearLinkedHashSet) {
  ClearCollection<HeapLinkedHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapLinkedHashSet) {
  SwapCollections<HeapLinkedHashSetAdapter<Member<IntegerObject>>>();
}

// HeapListHashSet

template <typename T>
class HeapListHashSetAdapter : public HeapListHashSet<T> {
 public:
  ALWAYS_INLINE void swap(HeapListHashSetAdapter<T>& other) {
    HeapListHashSet<T>::Swap(other);
  }
};

TEST_F(ConcurrentMarkingTest, AddToListHashSet) {
  AddToCollection<HeapListHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfListHashSet) {
  RemoveFromBeginningOfCollection<
      HeapListHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfListHashSet) {
  RemoveFromMiddleOfCollection<HeapListHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfListHashSet) {
  RemoveFromEndOfCollection<HeapListHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearListHashSet) {
  ClearCollection<HeapListHashSetAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapListHashSet) {
  SwapCollections<HeapListHashSetAdapter<Member<IntegerObject>>>();
}

// HeapHashCountedSet

TEST_F(ConcurrentMarkingTest, AddToHashCountedSet) {
  AddToCollection<HeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfHashCountedSet) {
  RemoveFromBeginningOfCollection<HeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfHashCountedSet) {
  RemoveFromMiddleOfCollection<HeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfHashCountedSet) {
  RemoveFromEndOfCollection<HeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearHashCountedSet) {
  ClearCollection<HeapHashCountedSet<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapHashCountedSet) {
  SwapCollections<HeapHashCountedSet<Member<IntegerObject>>>();
}

// HeapVector

// Additional test for vectors and deques
template <typename V>
void PopFromCollection() {
  constexpr int kIterations = 10;
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectionWrapper<V>> persistent =
      MakeGarbageCollected<CollectionWrapper<V>>();
  V* vector = persistent->GetCollection();
  for (int i = 0; i < (kIterations * kIterations); ++i) {
    vector->insert(MakeGarbageCollected<IntegerObject>(i));
  }
  driver.Start();
  for (int i = 0; i < kIterations; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < kIterations; ++j) {
      vector->pop_back();
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

#define TEST_VECTOR_COLLECTION(name, type)                                \
  TEST_F(ConcurrentMarkingTest, AddTo##name) { AddToCollection<type>(); } \
  TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOf##name) {            \
    RemoveFromBeginningOfCollection<type>();                              \
  }                                                                       \
  TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOf##name) {               \
    RemoveFromMiddleOfCollection<type>();                                 \
  }                                                                       \
  TEST_F(ConcurrentMarkingTest, RemoveFromEndOf##name) {                  \
    RemoveFromEndOfCollection<type>();                                    \
  }                                                                       \
  TEST_F(ConcurrentMarkingTest, Clear##name) { ClearCollection<type>(); } \
  TEST_F(ConcurrentMarkingTest, Swap##name) { SwapCollections<type>(); }  \
  TEST_F(ConcurrentMarkingTest, PopFrom##name) { PopFromCollection<type>(); }

template <typename T, wtf_size_t inlineCapacity = 0>
class HeapVectorAdapter : public HeapVector<T, inlineCapacity> {
  using Base = HeapVector<T, inlineCapacity>;

 public:
  template <typename U>
  ALWAYS_INLINE void insert(U* u) {
    Base::push_back(u);
  }
};

TEST_F(ConcurrentMarkingTest, AddToVector) {
  AddToCollection<HeapVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfVector) {
  RemoveFromBeginningOfCollection<HeapVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfVector) {
  RemoveFromMiddleOfCollection<HeapVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfVector) {
  RemoveFromEndOfCollection<HeapVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearVector) {
  ClearCollection<HeapVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapVector) {
  SwapCollections<HeapVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, PopFromVector) {
  PopFromCollection<HeapVectorAdapter<Member<IntegerObject>>>();
}

// HeapVector with inlined buffer

template <typename T>
class HeapInlinedVectorAdapter : public HeapVectorAdapter<T, 10> {};

TEST_F(ConcurrentMarkingTest, AddToInlinedVector) {
  AddToCollection<HeapInlinedVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfInlinedVector) {
  RemoveFromBeginningOfCollection<
      HeapInlinedVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfInlinedVector) {
  RemoveFromMiddleOfCollection<
      HeapInlinedVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfInlinedVector) {
  RemoveFromEndOfCollection<HeapInlinedVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearInlinedVector) {
  ClearCollection<HeapInlinedVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapInlinedVector) {
  SwapCollections<HeapInlinedVectorAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, PopFromInlinedVector) {
  PopFromCollection<HeapInlinedVectorAdapter<Member<IntegerObject>>>();
}

// HeapVector of std::pairs

template <typename T>
class HeapVectorOfPairsAdapter : public HeapVector<std::pair<T, T>> {
  using Base = HeapVector<std::pair<T, T>>;

 public:
  template <typename U>
  ALWAYS_INLINE void insert(U* u) {
    Base::push_back(std::make_pair<T, T>(u, u));
  }
};

TEST_F(ConcurrentMarkingTest, AddToVectorOfPairs) {
  AddToCollection<HeapVectorOfPairsAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfVectorOfPairs) {
  RemoveFromBeginningOfCollection<
      HeapVectorOfPairsAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfVectorOfPairs) {
  RemoveFromMiddleOfCollection<
      HeapVectorOfPairsAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfVectorOfPairs) {
  RemoveFromEndOfCollection<HeapVectorOfPairsAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearVectorOfPairs) {
  ClearCollection<HeapVectorOfPairsAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapVectorOfPairs) {
  SwapCollections<HeapVectorOfPairsAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, PopFromVectorOfPairs) {
  PopFromCollection<HeapVectorOfPairsAdapter<Member<IntegerObject>>>();
}

// HeapDeque

template <typename T>
class HeapDequeAdapter : public HeapDeque<T> {
 public:
  template <typename U>
  ALWAYS_INLINE void insert(U* u) {
    HeapDeque<T>::push_back(u);
  }
  ALWAYS_INLINE void erase(typename HeapDeque<T>::iterator) {
    HeapDeque<T>::pop_back();
  }
  ALWAYS_INLINE void swap(HeapDequeAdapter<T>& other) {
    HeapDeque<T>::Swap(other);
  }
};

TEST_F(ConcurrentMarkingTest, AddToDeque) {
  AddToCollection<HeapDequeAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromBeginningOfDeque) {
  RemoveFromBeginningOfCollection<HeapDequeAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromMiddleOfDeque) {
  RemoveFromMiddleOfCollection<HeapDequeAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, RemoveFromEndOfDeque) {
  RemoveFromEndOfCollection<HeapDequeAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, ClearDeque) {
  ClearCollection<HeapDequeAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, SwapDeque) {
  SwapCollections<HeapDequeAdapter<Member<IntegerObject>>>();
}
TEST_F(ConcurrentMarkingTest, PopFromDeque) {
  PopFromCollection<HeapDequeAdapter<Member<IntegerObject>>>();
}

namespace {

class RegisteredMixin;

class CollectsMixins : public GarbageCollected<CollectsMixins> {
  using MixinSet = HeapHashSet<Member<RegisteredMixin>>;

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
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  Persistent<CollectsMixins> collector = MakeGarbageCollected<CollectsMixins>();
  driver.Start();
  for (int i = 0; i < kIterations; ++i) {
    driver.SingleConcurrentStep();
    for (int j = 0; j < kIterations; ++j) {
      MakeGarbageCollected<GCedWithRegisteredMixin>(collector.Get());
    }
  }
  driver.FinishSteps();
  driver.FinishGC();
}

}  // namespace concurrent_marking_test
}  // namespace blink

#endif  // defined(THREAD_SANITIZER)
