// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <iostream>
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/hash_traits.h"

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

class EphemeronCallbacksCounter
    : public GarbageCollected<EphemeronCallbacksCounter> {
 public:
  EphemeronCallbacksCounter(size_t* count_holder)
      : count_holder_(count_holder) {}

  void Trace(Visitor* visitor) {
    visitor->RegisterWeakCallbackMethod<EphemeronCallbacksCounter,
                                        &EphemeronCallbacksCounter::Callback>(
        this);
  }

  void Callback(const WeakCallbackInfo& info) {
    *count_holder_ = ThreadState::Current()->Heap().ephemeron_callbacks_.size();
  }

 private:
  size_t* count_holder_;
};

TEST_F(WeaknessMarkingTest, UntracableEphemeronIsNotRegsitered) {
  size_t ephemeron_count;
  Persistent<EphemeronCallbacksCounter> ephemeron_callbacks_counter =
      MakeGarbageCollected<EphemeronCallbacksCounter>(&ephemeron_count);
  TestSupportingGC::PreciselyCollectGarbage();
  size_t old_ephemeron_count = ephemeron_count;
  using Map = HeapHashMap<WeakMember<IntegerObject>, int>;
  Persistent<Map> map = MakeGarbageCollected<Map>();
  map->insert(MakeGarbageCollected<IntegerObject>(1), 2);
  TestSupportingGC::PreciselyCollectGarbage();
  // Ephemeron value is not traceable, thus the map shouldn't be treated as an
  // ephemeron.
  EXPECT_EQ(old_ephemeron_count, ephemeron_count);
}

TEST_F(WeaknessMarkingTest, TracableEphemeronIsRegsitered) {
  size_t ephemeron_count;
  Persistent<EphemeronCallbacksCounter> ephemeron_callbacks_counter =
      MakeGarbageCollected<EphemeronCallbacksCounter>(&ephemeron_count);
  TestSupportingGC::PreciselyCollectGarbage();
  size_t old_ephemeron_count = ephemeron_count;
  using Map = HeapHashMap<WeakMember<IntegerObject>, Member<IntegerObject>>;
  Persistent<Map> map = MakeGarbageCollected<Map>();
  map->insert(MakeGarbageCollected<IntegerObject>(1),
              MakeGarbageCollected<IntegerObject>(2));
  TestSupportingGC::PreciselyCollectGarbage();
  EXPECT_NE(old_ephemeron_count, ephemeron_count);
}

}  // namespace weakness_marking_test

}  // namespace blink
