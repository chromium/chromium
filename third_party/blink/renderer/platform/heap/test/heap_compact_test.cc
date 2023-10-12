// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_deque.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <memory>

namespace {

class IntWrapper : public blink::GarbageCollected<IntWrapper> {
 public:
  static IntWrapper* Create(int x) {
    return blink::MakeGarbageCollected<IntWrapper>(x);
  }

  virtual ~IntWrapper() = default;

  void Trace(blink::Visitor* visitor) const {
  }

  int Value() const { return x_; }

  bool operator==(const IntWrapper& other) const {
    return other.Value() == Value();
  }

  unsigned GetHash() { return WTF::GetHash(x_); }

  explicit IntWrapper(int x) : x_(x) {}

 private:
  IntWrapper() = delete;

  int x_;
};

static_assert(WTF::IsTraceable<IntWrapper>::value,
              "IsTraceable<> template failed to recognize trace method.");

}  // namespace

using IntVector = blink::HeapVector<blink::Member<IntWrapper>>;
using IntDeque = blink::HeapDeque<blink::Member<IntWrapper>>;
using IntMap = blink::HeapHashMap<blink::Member<IntWrapper>, int>;
// TODO(sof): decide if this ought to be a global trait specialization.
// (i.e., for HeapHash*<T>.)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(IntVector)

namespace blink {

class HeapCompactTest : public TestSupportingGC {
 public:
  void PerformHeapCompaction() {
    CompactionTestDriver(ThreadState::Current()).ForceCompactionForNextGC();
    PreciselyCollectGarbage();
  }
};

TEST_F(HeapCompactTest, CompactVector) {
  ClearOutOldGarbage();

  IntWrapper* val = IntWrapper::Create(1);
  Persistent<IntVector> vector = MakeGarbageCollected<IntVector>(10, val);
  EXPECT_EQ(10u, vector->size());

  for (IntWrapper* item : *vector)
    EXPECT_EQ(val, item);

  PerformHeapCompaction();

  for (IntWrapper* item : *vector)
    EXPECT_EQ(val, item);
}

TEST_F(HeapCompactTest, CompactHashMap) {
  ClearOutOldGarbage();

  Persistent<IntMap> int_map = MakeGarbageCollected<IntMap>();
  for (wtf_size_t i = 0; i < 100; ++i) {
    IntWrapper* val = IntWrapper::Create(i);
    int_map->insert(val, 100 - i);
  }

  EXPECT_EQ(100u, int_map->size());
  for (auto k : *int_map)
    EXPECT_EQ(k.key->Value(), 100 - k.value);

  PerformHeapCompaction();

  for (auto k : *int_map)
    EXPECT_EQ(k.key->Value(), 100 - k.value);
}

TEST_F(HeapCompactTest, CompactVectorOfVector) {
  ClearOutOldGarbage();

  using IntVectorVector = HeapVector<IntVector>;

  Persistent<IntVectorVector> int_vector_vector =
      MakeGarbageCollected<IntVectorVector>();
  for (size_t i = 0; i < 10; ++i) {
    IntVector vector;
    for (wtf_size_t j = 0; j < 10; ++j) {
      IntWrapper* val = IntWrapper::Create(j);
      vector.push_back(val);
    }
    int_vector_vector->push_back(vector);
  }

  EXPECT_EQ(10u, int_vector_vector->size());
  {
    int i = 0;
    for (auto vector : *int_vector_vector) {
      EXPECT_EQ(10u, vector.size());
      for (auto item : vector) {
        EXPECT_EQ(item->Value(), i % 10);
        i++;
      }
    }
  }

  PerformHeapCompaction();

  {
    int i = 0;
    EXPECT_EQ(10u, int_vector_vector->size());
    for (auto vector : *int_vector_vector) {
      EXPECT_EQ(10u, vector.size());
      for (auto item : vector) {
        EXPECT_EQ(item->Value(), i % 10);
        i++;
      }
    }
  }
}

TEST_F(HeapCompactTest, CompactHashPartVector) {
  ClearOutOldGarbage();

  using IntVectorMap = HeapHashMap<int, Member<IntVector>>;

  Persistent<IntVectorMap> int_vector_map =
      MakeGarbageCollected<IntVectorMap>();
  for (wtf_size_t i = 0; i < 10; ++i) {
    IntVector* vector = MakeGarbageCollected<IntVector>();
    for (wtf_size_t j = 0; j < 10; ++j) {
      vector->push_back(IntWrapper::Create(j));
    }
    int_vector_map->insert(1 + i, vector);
  }

  EXPECT_EQ(10u, int_vector_map->size());
  for (const IntVector* int_vector : int_vector_map->Values()) {
    EXPECT_EQ(10u, int_vector->size());
    for (wtf_size_t i = 0; i < int_vector->size(); ++i) {
      EXPECT_EQ(static_cast<int>(i), (*int_vector)[i]->Value());
    }
  }

  PerformHeapCompaction();

  EXPECT_EQ(10u, int_vector_map->size());
  for (const IntVector* int_vector : int_vector_map->Values()) {
    EXPECT_EQ(10u, int_vector->size());
    for (wtf_size_t i = 0; i < int_vector->size(); ++i) {
      EXPECT_EQ(static_cast<int>(i), (*int_vector)[i]->Value());
    }
  }
}

TEST_F(HeapCompactTest, CompactDeques) {
  Persistent<IntDeque> deque = MakeGarbageCollected<IntDeque>();
  for (int i = 0; i < 8; ++i) {
    deque->push_front(IntWrapper::Create(i));
  }
  EXPECT_EQ(8u, deque->size());

  for (wtf_size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i)->Value());

  PerformHeapCompaction();

  for (wtf_size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i)->Value());
}

TEST_F(HeapCompactTest, CompactLinkedHashSet) {
  using OrderedHashSet = HeapLinkedHashSet<Member<IntWrapper>>;
  Persistent<OrderedHashSet> set = MakeGarbageCollected<OrderedHashSet>();
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::Create(i);
    set->insert(value);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (IntWrapper* v : *set) {
    EXPECT_EQ(expected, v->Value());
    expected++;
  }

  for (int i = 1; i < 13; i += 2) {
    auto it = set->begin();
    for (int j = 0; j < (i + 1) / 2; ++j) {
      ++it;
    }
    set->erase(it);
  }
  EXPECT_EQ(7u, set->size());

  expected = 0;
  for (IntWrapper* v : *set) {
    EXPECT_EQ(expected, v->Value());
    expected += 2;
  }

  PerformHeapCompaction();

  expected = 0;
  for (IntWrapper* v : *set) {
    EXPECT_EQ(expected, v->Value());
    expected += 2;
  }
  EXPECT_EQ(7u, set->size());
}

TEST_F(HeapCompactTest, CompactLinkedHashSetVector) {
  using OrderedHashSet = HeapLinkedHashSet<Member<IntVector>>;
  Persistent<OrderedHashSet> set = MakeGarbageCollected<OrderedHashSet>();
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::Create(i);
    IntVector* vector = MakeGarbageCollected<IntVector>(19, value);
    set->insert(vector);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (IntVector* v : *set) {
    EXPECT_EQ(expected, (*v)[0]->Value());
    expected++;
  }

  PerformHeapCompaction();

  expected = 0;
  for (IntVector* v : *set) {
    EXPECT_EQ(expected, (*v)[0]->Value());
    expected++;
  }
}

TEST_F(HeapCompactTest, CompactLinkedHashSetMap) {
  using Inner = HeapHashSet<Member<IntWrapper>>;
  using OrderedHashSet = HeapLinkedHashSet<Member<Inner>>;

  Persistent<OrderedHashSet> set = MakeGarbageCollected<OrderedHashSet>();
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::Create(i);
    Inner* inner = MakeGarbageCollected<Inner>();
    inner->insert(value);
    set->insert(inner);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->Value());
    expected++;
  }

  PerformHeapCompaction();

  expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->Value());
    expected++;
  }
}

TEST_F(HeapCompactTest, CompactLinkedHashSetNested) {
  using Inner = HeapLinkedHashSet<Member<IntWrapper>>;
  using OrderedHashSet = HeapLinkedHashSet<Member<Inner>>;

  Persistent<OrderedHashSet> set = MakeGarbageCollected<OrderedHashSet>();
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::Create(i);
    Inner* inner = MakeGarbageCollected<Inner>();
    inner->insert(value);
    set->insert(inner);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->Value());
    expected++;
  }

  PerformHeapCompaction();

  expected = 0;
  for (const Inner* v : *set) {
    EXPECT_EQ(1u, v->size());
    EXPECT_EQ(expected, (*v->begin())->Value());
    expected++;
  }
}

TEST_F(HeapCompactTest, CompactInlinedBackingStore) {
  // Regression test: https://crbug.com/875044
  //
  // This test checks that compaction properly updates pointers to statically
  // allocated inline backings, see e.g. Vector::inline_buffer_.

  // Use a Key with pre-defined hash traits.
  using Key = Member<IntWrapper>;
  // Value uses a statically allocated inline backing of size 64. As long as no
  // more than elements are added no out-of-line allocation is triggered.
  // The internal forwarding pointer to the inlined storage needs to be handled
  // by compaction.
  using Value = HeapVector<Member<IntWrapper>, 64>;
  using MapWithInlinedBacking = HeapHashMap<Key, Member<Value>>;

  Persistent<MapWithInlinedBacking> map =
      MakeGarbageCollected<MapWithInlinedBacking>();
  {
    // Create a map that is reclaimed during compaction.
    (MakeGarbageCollected<MapWithInlinedBacking>())
        ->insert(IntWrapper::Create(1), MakeGarbageCollected<Value>());

    IntWrapper* wrapper = IntWrapper::Create(1);
    Value* storage = MakeGarbageCollected<Value>();
    storage->push_front(wrapper);
    map->insert(wrapper, std::move(storage));
  }
  PerformHeapCompaction();
  // The first GC should update the pointer accordingly and thus not crash on
  // the second GC.
  PerformHeapCompaction();
}

struct Dummy final {};

struct NestedType final {
  DISALLOW_NEW();

  static size_t num_dtor_checks;

  NestedType() {
    vec.emplace_back();
    CHECK_EQ(vec.size(), 1u);
    CheckValidInlineBuffer();
  }
  ~NestedType() {
    if (vec.size() > 0) {
      num_dtor_checks++;
      CheckValidInlineBuffer();
    }
  }

  void CheckValidInlineBuffer() const {
    if (!Vector<Dummy, 4>::SupportsInlineCapacity()) {
      return;
    }

    const auto front = reinterpret_cast<uintptr_t>(&vec.front());
    // Since the vector has inline capacity, the front must be somewhere within
    // the vector itself.
    CHECK(reinterpret_cast<uintptr_t>(&vec) <= front &&
          front < reinterpret_cast<uintptr_t>(&vec) + sizeof(vec));
  }

  void Trace(Visitor* visitor) const {}

  Vector<Dummy, 4> vec;
};

size_t NestedType::num_dtor_checks = 0;

}  // namespace blink

namespace WTF {
template <>
struct VectorTraits<blink::NestedType> : VectorTraitsBase<blink::NestedType> {
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};
}  // namespace WTF

namespace blink {

TEST_F(HeapCompactTest, AvoidCompactionWhenTraitsProhibitMemcpy) {
  // Regression test: https://crbug.com/1478343
  //
  // This test checks that compaction does not happen in cases where
  // `VectorTraits<T>::kCanMoveWithMemcpy` doesn't hold.

  static_assert(WTF::VectorTraits<NestedType>::kCanMoveWithMemcpy == false,
                "should not allow move using memcpy");
  // Create a vector with a backing store that immediately gets reclaimed. The
  // backing store leaves free memory to be reused for compaction.
  MakeGarbageCollected<HeapVector<NestedType>>()->emplace_back();
  // The vector that is actually connected.
  Persistent<HeapVector<NestedType>> vec =
      MakeGarbageCollected<HeapVector<NestedType>>();
  vec->emplace_back();
  PerformHeapCompaction();
  vec = nullptr;
  PreciselyCollectGarbage();
  PreciselyCollectGarbage();
  EXPECT_EQ(NestedType::num_dtor_checks, 2u);
}

}  // namespace blink
