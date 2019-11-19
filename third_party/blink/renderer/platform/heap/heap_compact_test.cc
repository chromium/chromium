// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/heap_compact.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <memory>

namespace {

enum VerifyArenaCompaction {
  NoVerify,
  VectorsAreCompacted,
  HashTablesAreCompacted,
};

class IntWrapper : public blink::GarbageCollected<IntWrapper> {
 public:
  static bool did_verify_at_least_once;

  static IntWrapper* Create(int x, VerifyArenaCompaction verify = NoVerify) {
    did_verify_at_least_once = false;
    return blink::MakeGarbageCollected<IntWrapper>(x, verify);
  }

  virtual ~IntWrapper() = default;

  void Trace(blink::Visitor* visitor) {
    // Verify if compaction is indeed activated.

    // There may be multiple passes over objects during a GC, even after
    // compaction is finished. Filter out that cases here.
    if (!visitor->Heap().Compaction()->IsCompacting())
      return;

    did_verify_at_least_once = true;
    // What arenas end up being compacted is dependent on residency,
    // so approximate the arena checks to fit.
    blink::HeapCompact* compaction = visitor->Heap().Compaction();
    switch (verify_) {
      case NoVerify:
        return;
      case HashTablesAreCompacted:
        CHECK(compaction->IsCompactingArena(
            blink::BlinkGC::kHashTableArenaIndex));
        return;
      case VectorsAreCompacted:
        CHECK(compaction->IsCompactingVectorArenasForTesting());
        return;
    }
  }

  int Value() const { return x_; }

  bool operator==(const IntWrapper& other) const {
    return other.Value() == Value();
  }

  unsigned GetHash() { return IntHash<int>::GetHash(x_); }

  IntWrapper(int x, VerifyArenaCompaction verify) : x_(x), verify_(verify) {}

 private:
  IntWrapper() = delete;

  int x_;
  VerifyArenaCompaction verify_;
};

bool IntWrapper::did_verify_at_least_once = false;

static_assert(WTF::IsTraceable<IntWrapper>::value,
              "IsTraceable<> template failed to recognize trace method.");

}  // namespace

using IntVector = blink::HeapVector<blink::Member<IntWrapper>>;
using IntDeque = blink::HeapDeque<blink::Member<IntWrapper>>;
using IntMap = blink::HeapHashMap<blink::Member<IntWrapper>, int>;
// TODO(sof): decide if this ought to be a global trait specialization.
// (i.e., for HeapHash*<T>.)
WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(IntMap)

namespace blink {

class HeapCompactTest : public TestSupportingGC {
 public:
  void PerformHeapCompaction() {
    ThreadState::Current()->EnableCompactionForNextGCForTesting();
    PreciselyCollectGarbage();
  }
};

TEST_F(HeapCompactTest, CompactVector) {
  ClearOutOldGarbage();

  IntWrapper* val = IntWrapper::Create(1, VectorsAreCompacted);
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
    IntWrapper* val = IntWrapper::Create(i, HashTablesAreCompacted);
    int_map->insert(val, 100 - i);
  }

  EXPECT_EQ(100u, int_map->size());
  for (auto k : *int_map)
    EXPECT_EQ(k.key->Value(), 100 - k.value);

  PerformHeapCompaction();
  EXPECT_TRUE(IntWrapper::did_verify_at_least_once);

  for (auto k : *int_map)
    EXPECT_EQ(k.key->Value(), 100 - k.value);
}

TEST_F(HeapCompactTest, CompactVectorPartHashMap) {
  ClearOutOldGarbage();

  using IntMapVector = HeapVector<IntMap>;

  Persistent<IntMapVector> int_map_vector =
      MakeGarbageCollected<IntMapVector>();
  for (size_t i = 0; i < 10; ++i) {
    IntMap map;
    for (wtf_size_t j = 0; j < 10; ++j) {
      IntWrapper* val = IntWrapper::Create(j, VectorsAreCompacted);
      map.insert(val, 10 - j);
    }
    int_map_vector->push_back(map);
  }

  EXPECT_EQ(10u, int_map_vector->size());
  for (auto map : *int_map_vector) {
    EXPECT_EQ(10u, map.size());
    for (auto k : map) {
      EXPECT_EQ(k.key->Value(), 10 - k.value);
    }
  }

  PerformHeapCompaction();
  EXPECT_TRUE(IntWrapper::did_verify_at_least_once);

  EXPECT_EQ(10u, int_map_vector->size());
  for (auto map : *int_map_vector) {
    EXPECT_EQ(10u, map.size());
    for (auto k : map) {
      EXPECT_EQ(k.key->Value(), 10 - k.value);
    }
  }
}

TEST_F(HeapCompactTest, CompactHashPartVector) {
  ClearOutOldGarbage();

  using IntVectorMap = HeapHashMap<int, IntVector>;

  Persistent<IntVectorMap> int_vector_map =
      MakeGarbageCollected<IntVectorMap>();
  for (wtf_size_t i = 0; i < 10; ++i) {
    IntVector vector;
    for (wtf_size_t j = 0; j < 10; ++j) {
      vector.push_back(IntWrapper::Create(j, HashTablesAreCompacted));
    }
    int_vector_map->insert(1 + i, vector);
  }

  EXPECT_EQ(10u, int_vector_map->size());
  for (const IntVector& int_vector : int_vector_map->Values()) {
    EXPECT_EQ(10u, int_vector.size());
    for (wtf_size_t i = 0; i < int_vector.size(); ++i) {
      EXPECT_EQ(static_cast<int>(i), int_vector[i]->Value());
    }
  }

  PerformHeapCompaction();
  EXPECT_TRUE(IntWrapper::did_verify_at_least_once);

  EXPECT_EQ(10u, int_vector_map->size());
  for (const IntVector& int_vector : int_vector_map->Values()) {
    EXPECT_EQ(10u, int_vector.size());
    for (wtf_size_t i = 0; i < int_vector.size(); ++i) {
      EXPECT_EQ(static_cast<int>(i), int_vector[i]->Value());
    }
  }
}

TEST_F(HeapCompactTest, CompactDeques) {
  Persistent<IntDeque> deque = MakeGarbageCollected<IntDeque>();
  for (int i = 0; i < 8; ++i) {
    deque->push_front(IntWrapper::Create(i, VectorsAreCompacted));
  }
  EXPECT_EQ(8u, deque->size());

  for (wtf_size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i)->Value());

  PerformHeapCompaction();
  EXPECT_TRUE(IntWrapper::did_verify_at_least_once);

  for (wtf_size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i)->Value());
}

TEST_F(HeapCompactTest, CompactDequeVectors) {
  Persistent<HeapDeque<IntVector>> deque =
      MakeGarbageCollected<HeapDeque<IntVector>>();
  for (int i = 0; i < 8; ++i) {
    IntWrapper* value = IntWrapper::Create(i, VectorsAreCompacted);
    IntVector vector = IntVector(8, value);
    deque->push_front(vector);
  }
  EXPECT_EQ(8u, deque->size());

  for (wtf_size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i).at(i)->Value());

  PerformHeapCompaction();
  EXPECT_TRUE(IntWrapper::did_verify_at_least_once);

  for (wtf_size_t i = 0; i < deque->size(); ++i)
    EXPECT_EQ(static_cast<int>(7 - i), deque->at(i).at(i)->Value());
}

TEST_F(HeapCompactTest, CompactLinkedHashSet) {
  using OrderedHashSet = HeapLinkedHashSet<Member<IntWrapper>>;
  Persistent<OrderedHashSet> set = MakeGarbageCollected<OrderedHashSet>();
  for (int i = 0; i < 13; ++i) {
    IntWrapper* value = IntWrapper::Create(i, HashTablesAreCompacted);
    set->insert(value);
  }
  EXPECT_EQ(13u, set->size());

  int expected = 0;
  for (IntWrapper* v : *set) {
    EXPECT_EQ(expected, v->Value());
    expected++;
  }

  PerformHeapCompaction();
  EXPECT_TRUE(IntWrapper::did_verify_at_least_once);

  expected = 0;
  for (IntWrapper* v : *set) {
    EXPECT_EQ(expected, v->Value());
    expected++;
  }
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
  EXPECT_TRUE(IntWrapper::did_verify_at_least_once);

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
  EXPECT_TRUE(IntWrapper::did_verify_at_least_once);

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
  EXPECT_TRUE(IntWrapper::did_verify_at_least_once);

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
  using MapWithInlinedBacking = HeapHashMap<Key, Value>;

  Persistent<MapWithInlinedBacking> map =
      MakeGarbageCollected<MapWithInlinedBacking>();
  {
    // Create a map that is reclaimed during compaction.
    (MakeGarbageCollected<MapWithInlinedBacking>())
        ->insert(IntWrapper::Create(1, HashTablesAreCompacted), Value());

    IntWrapper* wrapper = IntWrapper::Create(1, HashTablesAreCompacted);
    Value storage;
    storage.push_front(wrapper);
    map->insert(wrapper, std::move(storage));
  }
  PerformHeapCompaction();
  // The first GC should update the pointer accordingly and thus not crash on
  // the second GC.
  PerformHeapCompaction();
}

}  // namespace blink
