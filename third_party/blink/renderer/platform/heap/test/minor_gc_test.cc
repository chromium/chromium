// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/cppgc/testing.h"

namespace blink {

namespace {

bool IsOld(void* object) {
  return cppgc::testing::IsHeapObjectOld(object);
}

class SimpleGCedBase : public GarbageCollected<SimpleGCedBase> {
 public:
  static size_t destructed_objects;

  virtual ~SimpleGCedBase() { ++destructed_objects; }

  void Trace(Visitor* v) const { v->Trace(next); }

  Member<SimpleGCedBase> next;
};

size_t SimpleGCedBase::destructed_objects;

template <size_t Size>
class SimpleGCed final : public SimpleGCedBase {
  char array[Size];
};

using Small = SimpleGCed<64>;
using Large = SimpleGCed<1024 * 1024>;

template <typename Type>
struct OtherType;
template <>
struct OtherType<Small> {
  using Type = Large;
};
template <>
struct OtherType<Large> {
  using Type = Small;
};

class MinorGCTest : public TestSupportingGC {
 public:
  MinorGCTest() {
    ClearOutOldGarbage();
    SimpleGCedBase::destructed_objects = 0;
  }

  static size_t DestructedObjects() {
    return SimpleGCedBase::destructed_objects;
  }

  static void CollectMinor() {
    ThreadState::Current()->CollectGarbageInYoungGenerationForTesting(
        ThreadState::StackState::kNoHeapPointers);
  }

  static void CollectMajor() {
    ThreadState::Current()->CollectAllGarbageForTesting(
        ThreadState::StackState::kNoHeapPointers);
  }
};

template <typename SmallOrLarge>
class MinorGCTestForType : public MinorGCTest {
 public:
  using Type = SmallOrLarge;
};

}  // namespace

using ObjectTypes = ::testing::Types<Small, Large>;
TYPED_TEST_SUITE(MinorGCTestForType, ObjectTypes);

TYPED_TEST(MinorGCTestForType, InterGenerationalPointerInCollection) {
  using Type = typename TestFixture::Type;

  static constexpr size_t kCollectionSize = 128;
  Persistent<HeapVector<Member<Type>>> old =
      MakeGarbageCollected<HeapVector<Member<Type>>>();
  old->resize(kCollectionSize);
  void* raw_backing = old->data();
  EXPECT_FALSE(IsOld(raw_backing));
  MinorGCTest::CollectMinor();
  EXPECT_TRUE(IsOld(raw_backing));

  // Issue barrier for every second member.
  size_t i = 0;
  for (auto& member : *old) {
    if (i % 2) {
      member = MakeGarbageCollected<Type>();
    } else {
      MakeGarbageCollected<Type>();
    }
    ++i;
  }

  // Check that the remembered set is visited.
  MinorGCTest::CollectMinor();
  EXPECT_EQ(kCollectionSize / 2, MinorGCTest::DestructedObjects());
  for (const auto& member : *old) {
    if (member) {
      EXPECT_TRUE(IsOld(member.Get()));
    }
  }

  old.Release();
  MinorGCTest::CollectMajor();
  EXPECT_EQ(kCollectionSize, MinorGCTest::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType, InterGenerationalPointerInPlaceBarrier) {
  using Type = typename TestFixture::Type;
  using ValueType = std::pair<WTF::String, Member<Type>>;
  using CollectionType = HeapVector<ValueType>;

  static constexpr size_t kCollectionSize = 1;

  Persistent<CollectionType> old = MakeGarbageCollected<CollectionType>();
  old->ReserveInitialCapacity(kCollectionSize);

  void* raw_backing = old->data();
  EXPECT_FALSE(IsOld(raw_backing));
  MinorGCTest::CollectMinor();
  EXPECT_TRUE(IsOld(raw_backing));

  // Issue barrier (in HeapAllocator::NotifyNewElement).
  old->push_back(std::make_pair("test", MakeGarbageCollected<Type>()));

  // Store the reference in a weak pointer to check liveness.
  WeakPersistent<Type> object_is_live = (*old)[0].second;

  // Check that the remembered set is visited.
  MinorGCTest::CollectMinor();

  // No objects destructed.
  EXPECT_EQ(0u, MinorGCTest::DestructedObjects());
  EXPECT_EQ(1u, old->size());

  {
    Type* member = (*old)[0].second;
    EXPECT_TRUE(IsOld(member));
    EXPECT_TRUE(object_is_live);
  }

  old.Release();
  MinorGCTest::CollectMajor();
  EXPECT_FALSE(object_is_live);
  EXPECT_EQ(1u, MinorGCTest::DestructedObjects());
}

TYPED_TEST(MinorGCTestForType,
           InterGenerationalPointerNotifyingBunchOfElements) {
  using Type = typename TestFixture::Type;
  using ValueType = std::pair<int, Member<Type>>;
  using CollectionType = HeapVector<ValueType>;
  static_assert(WTF::VectorTraits<ValueType>::kCanCopyWithMemcpy,
                "Only when copying with memcpy the "
                "Allocator::NotifyNewElements is called");

  Persistent<CollectionType> old = MakeGarbageCollected<CollectionType>();
  old->ReserveInitialCapacity(1);

  void* raw_backing = old->data();
  EXPECT_FALSE(IsOld(raw_backing));

  // Mark old backing.
  MinorGCTest::CollectMinor();
  EXPECT_TRUE(IsOld(raw_backing));

  Persistent<CollectionType> young = MakeGarbageCollected<CollectionType>();

  // Add a single element to the young container.
  young->push_back(std::make_pair(1, MakeGarbageCollected<Type>()));

  // Store the reference in a weak pointer to check liveness.
  WeakPersistent<Type> object_is_live = (*young)[0].second;

  // Copy young container and issue barrier in HeapAllocator::NotifyNewElements.
  *old = *young;

  // Release young container.
  young.Release();

  // Check that the remembered set is visited.
  MinorGCTest::CollectMinor();

  // Nothing must be destructed since the old vector backing was revisited.
  EXPECT_EQ(0u, MinorGCTest::DestructedObjects());
  EXPECT_EQ(1u, old->size());

  {
    Type* member = (*old)[0].second;
    EXPECT_TRUE(IsOld(member));
    EXPECT_TRUE(object_is_live);
  }

  old.Release();
  MinorGCTest::CollectMajor();
  EXPECT_FALSE(object_is_live);
  EXPECT_EQ(1u, MinorGCTest::DestructedObjects());
}

namespace {
template <typename T>
class InlinedObjectBase {
  DISALLOW_NEW();

 public:
  InlinedObjectBase() : value_(MakeGarbageCollected<T>()) {}
  virtual ~InlinedObjectBase() = default;

  void Trace(Visitor* visitor) const { visitor->Trace(value_); }

  Member<T> GetValue() const { return value_; }

 private:
  int a = 0;
  Member<T> value_;
};

template <typename T>
class InlinedObject : public InlinedObjectBase<T> {};
}  // namespace

TYPED_TEST(MinorGCTestForType,
           InterGenerationalPointerInPlaceBarrierForTraced) {
  using Type = typename TestFixture::Type;
  using ValueType = InlinedObject<Type>;
  using CollectionType = HeapVector<ValueType>;

  static constexpr size_t kCollectionSize = 1;

  Persistent<CollectionType> old = MakeGarbageCollected<CollectionType>();
  old->ReserveInitialCapacity(kCollectionSize);

  void* raw_backing = old->data();
  EXPECT_FALSE(IsOld(raw_backing));
  MinorGCTest::CollectMinor();
  EXPECT_TRUE(IsOld(raw_backing));

  // Issue barrier (in HeapAllocator::NotifyNewElement).
  old->push_back(ValueType{});

  // Store the reference in a weak pointer to check liveness.
  WeakPersistent<Type> object_is_live = old->at(0).GetValue();

  // Check that the remembered set is visited.
  MinorGCTest::CollectMinor();

  // No objects destructed.
  EXPECT_EQ(0u, MinorGCTest::DestructedObjects());
  EXPECT_EQ(1u, old->size());

  {
    Type* member = old->at(0).GetValue();
    EXPECT_TRUE(IsOld(member));
    EXPECT_TRUE(object_is_live);
  }

  old.Release();
  MinorGCTest::CollectMajor();
  EXPECT_FALSE(object_is_live);
  EXPECT_EQ(1u, MinorGCTest::DestructedObjects());
}

}  // namespace blink
