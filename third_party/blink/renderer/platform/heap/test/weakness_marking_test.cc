// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_hash_set.h"
#include "third_party/blink/renderer/platform/heap/heap_test_objects.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {

class WeaknessMarkingTest : public TestSupportingGC {};

}  // namespace

enum class ObjectLiveness { Alive = 0, Dead };

template <typename Map,
          template <typename T>
          class KeyHolder,
          template <typename T>
          class ValueHolder>
void TestMapImpl(ObjectLiveness expected_key_liveness,
                 ObjectLiveness expected_value_liveness) {
  Persistent<Map> map = MakeGarbageCollected<Map>();
  KeyHolder<IntegerObject> int_key = MakeGarbageCollected<IntegerObject>(1);
  ValueHolder<IntegerObject> int_value = MakeGarbageCollected<IntegerObject>(2);
  map->insert(int_key.Get(), int_value.Get());
  TestSupportingGC::PreciselyCollectGarbage();
  if (expected_key_liveness == ObjectLiveness::Alive) {
    EXPECT_TRUE(int_key.Get());
  } else {
    EXPECT_FALSE(int_key.Get());
  }
  if (expected_value_liveness == ObjectLiveness::Alive) {
    EXPECT_TRUE(int_value.Get());
  } else {
    EXPECT_FALSE(int_value.Get());
  }
  EXPECT_EQ(((expected_key_liveness == ObjectLiveness::Alive) &&
             (expected_value_liveness == ObjectLiveness::Alive))
                ? 1u
                : 0u,
            map->size());
}

TEST_F(WeaknessMarkingTest, WeakToWeakMap) {
  using Map = HeapHashMap<WeakMember<IntegerObject>, WeakMember<IntegerObject>>;
  TestMapImpl<Map, Persistent, Persistent>(ObjectLiveness::Alive,
                                           ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, Persistent>(ObjectLiveness::Dead,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, Persistent, WeakPersistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Dead);
  TestMapImpl<Map, WeakPersistent, WeakPersistent>(ObjectLiveness::Dead,
                                                   ObjectLiveness::Dead);
}

TEST_F(WeaknessMarkingTest, WeakToStrongMap) {
  using Map = HeapHashMap<WeakMember<IntegerObject>, Member<IntegerObject>>;
  TestMapImpl<Map, Persistent, Persistent>(ObjectLiveness::Alive,
                                           ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, Persistent>(ObjectLiveness::Dead,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, Persistent, WeakPersistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, WeakPersistent>(ObjectLiveness::Dead,
                                                   ObjectLiveness::Dead);
}

TEST_F(WeaknessMarkingTest, StrongToWeakMap) {
  using Map = HeapHashMap<Member<IntegerObject>, WeakMember<IntegerObject>>;
  TestMapImpl<Map, Persistent, Persistent>(ObjectLiveness::Alive,
                                           ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, Persistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, Persistent, WeakPersistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Dead);
  TestMapImpl<Map, WeakPersistent, WeakPersistent>(ObjectLiveness::Dead,
                                                   ObjectLiveness::Dead);
}

TEST_F(WeaknessMarkingTest, StrongToStrongMap) {
  using Map = HeapHashMap<Member<IntegerObject>, Member<IntegerObject>>;
  TestMapImpl<Map, Persistent, Persistent>(ObjectLiveness::Alive,
                                           ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, Persistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, Persistent, WeakPersistent>(ObjectLiveness::Alive,
                                               ObjectLiveness::Alive);
  TestMapImpl<Map, WeakPersistent, WeakPersistent>(ObjectLiveness::Alive,
                                                   ObjectLiveness::Alive);
}

template <typename Set, template <typename T> class Type>
void TestSetImpl(ObjectLiveness object_liveness) {
  Persistent<Set> set = MakeGarbageCollected<Set>();
  Type<IntegerObject> object = MakeGarbageCollected<IntegerObject>(1);
  set->insert(object.Get());
  TestSupportingGC::PreciselyCollectGarbage();
  if (object_liveness == ObjectLiveness::Alive) {
    EXPECT_TRUE(object.Get());
  } else {
    EXPECT_FALSE(object.Get());
  }
  EXPECT_EQ((object_liveness == ObjectLiveness::Alive) ? 1u : 0u, set->size());
}

TEST_F(WeaknessMarkingTest, WeakSet) {
  using Set = HeapHashSet<WeakMember<IntegerObject>>;
  TestSetImpl<Set, Persistent>(ObjectLiveness::Alive);
  TestSetImpl<Set, WeakPersistent>(ObjectLiveness::Dead);
}

TEST_F(WeaknessMarkingTest, StrongSet) {
  using Set = HeapHashSet<Member<IntegerObject>>;
  TestSetImpl<Set, Persistent>(ObjectLiveness::Alive);
  TestSetImpl<Set, WeakPersistent>(ObjectLiveness::Alive);
}

TEST_F(WeaknessMarkingTest, DeadValueInReverseEphemeron) {
  using Map = HeapHashMap<Member<IntegerObject>, WeakMember<IntegerObject>>;
  Persistent<Map> map = MakeGarbageCollected<Map>();
  Persistent<IntegerObject> key = MakeGarbageCollected<IntegerObject>(1);
  map->insert(key.Get(), MakeGarbageCollected<IntegerObject>(2));
  EXPECT_EQ(1u, map->size());
  TestSupportingGC::PreciselyCollectGarbage();
  // Entries with dead values are removed.
  EXPECT_EQ(0u, map->size());
}

TEST_F(WeaknessMarkingTest, NullValueInReverseEphemeron) {
  using Map = HeapHashMap<Member<IntegerObject>, WeakMember<IntegerObject>>;
  Persistent<Map> map = MakeGarbageCollected<Map>();
  Persistent<IntegerObject> key = MakeGarbageCollected<IntegerObject>(1);
  map->insert(key.Get(), nullptr);
  EXPECT_EQ(1u, map->size());
  TestSupportingGC::PreciselyCollectGarbage();
  // Entries with null values are kept.
  EXPECT_EQ(1u, map->size());
}

namespace weakness_marking_test {

TEST_F(WeaknessMarkingTest, SwapIntoAlreadyProcessedWeakSet) {
  // Regression test: https://crbug.com/1038623
  //
  // Test ensures that an empty weak set that has already been marked sets up
  // weakness callbacks. This is important as another backing may be swapped in
  // at some point after marking it initially.
  using WeakLinkedSet = HeapLinkedHashSet<WeakMember<IntegerObject>>;
  Persistent<WeakLinkedSet> holder3(MakeGarbageCollected<WeakLinkedSet>());
  Persistent<WeakLinkedSet> holder4(MakeGarbageCollected<WeakLinkedSet>());
  holder3->insert(MakeGarbageCollected<IntegerObject>(1));
  IncrementalMarkingTestDriver driver2(ThreadState::Current());
  driver2.StartGC();
  driver2.TriggerMarkingSteps();
  holder3->Swap(*holder4.Get());
  driver2.FinishGC();
}

TEST_F(WeaknessMarkingTest, EmptyEphemeronCollection) {
  // Tests that an empty ephemeron collection does not crash in the GC when
  // processing a non-existent backing store.
  using Map = HeapHashMap<Member<IntegerObject>, WeakMember<IntegerObject>>;
  Persistent<Map> map = MakeGarbageCollected<Map>();
  TestSupportingGC::PreciselyCollectGarbage();
}

TEST_F(WeaknessMarkingTest, ClearWeakHashTableAfterMarking) {
  // Regression test: https://crbug.com/1054363
  //
  // Test ensures that no marked backing with weak pointers to dead object is
  // left behind after marking. The test creates a backing that is floating
  // garbage. The marking verifier ensures that all buckets are properly
  // deleted.
  using Set = HeapHashSet<WeakMember<IntegerObject>>;
  Persistent<Set> holder(MakeGarbageCollected<Set>());
  holder->insert(MakeGarbageCollected<IntegerObject>(1));
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  driver.TriggerMarkingSteps();
  holder->clear();
  driver.FinishGC();
}

TEST_F(WeaknessMarkingTest, StrongifyBackingOnStack) {
  // Test eunsures that conservative GC strongifies the backing store of
  // on-stack HeapLinkedHashSet.
  using WeakSet = HeapLinkedHashSet<WeakMember<IntegerObject>>;
  using StrongSet = HeapLinkedHashSet<Member<IntegerObject>>;
  WeakSet weak_set_on_stack;
  weak_set_on_stack.insert(MakeGarbageCollected<IntegerObject>(1));
  StrongSet strong_set_on_stack;
  strong_set_on_stack.insert(MakeGarbageCollected<IntegerObject>(1));
  TestSupportingGC::ConservativelyCollectGarbage();
  EXPECT_EQ(1u, weak_set_on_stack.size());
  EXPECT_EQ(1u, strong_set_on_stack.size());
  EXPECT_EQ(1, weak_set_on_stack.begin()->Get()->Value());
  EXPECT_EQ(1, strong_set_on_stack.begin()->Get()->Value());
}

TEST_F(WeaknessMarkingTest, StrongifyAlreadyMarkedOnBackingDuringIteration) {
  using WeakSet = HeapHashSet<WeakMember<IntegerObject>>;
  static constexpr size_t kNumberOfWeakEntries = 1000;

  Persistent<WeakSet> weak_set = MakeGarbageCollected<WeakSet>();
  for (size_t i = 0; i < kNumberOfWeakEntries; i++) {
    weak_set->insert(MakeGarbageCollected<IntegerObject>(i));
  }
  CHECK_EQ(weak_set->size(), kNumberOfWeakEntries);
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.StartGC();
  driver.TriggerMarkingSteps();
  bool trigger_gc = true;
  for (auto& it : *weak_set.Get()) {
    if (trigger_gc) {
      TestSupportingGC::ConservativelyCollectGarbage();
      trigger_gc = false;
      (void)it;
    }
  }
  CHECK_EQ(weak_set->size(), kNumberOfWeakEntries);
}

}  // namespace weakness_marking_test

}  // namespace blink
