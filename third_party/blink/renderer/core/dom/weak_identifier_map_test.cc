// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/weak_identifier_map.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class WeakIdentifierMapTest : public ::testing::Test {
 public:
  class TestClass final : public GarbageCollected<TestClass> {
   public:
    virtual void Trace(Visitor*) {}
  };

  using TestMap = WeakIdentifierMap<TestClass>;

  void SetUp() override;
  void TearDown() override;

  void CollectGarbage() {
    ThreadState::Current()->CollectAllGarbageForTesting(
        BlinkGC::kNoHeapPointersOnStack);
  }
};

DECLARE_WEAK_IDENTIFIER_MAP(WeakIdentifierMapTest::TestClass);
DEFINE_WEAK_IDENTIFIER_MAP(WeakIdentifierMapTest::TestClass)

void WeakIdentifierMapTest::SetUp() {
  EXPECT_EQ(0u, TestMap::GetSizeForTesting());
}

void WeakIdentifierMapTest::TearDown() {
  CollectGarbage();
  EXPECT_EQ(0u, TestMap::GetSizeForTesting());
}

TEST_F(WeakIdentifierMapTest, Basic) {
  auto* a = MakeGarbageCollected<TestClass>();
  auto* b = MakeGarbageCollected<TestClass>();

  auto id_a = TestMap::Identifier(a);
  EXPECT_NE(0, id_a);
  EXPECT_EQ(id_a, TestMap::Identifier(a));
  EXPECT_EQ(a, TestMap::Lookup(id_a));

  auto id_b = TestMap::Identifier(b);
  EXPECT_NE(0, id_b);
  EXPECT_NE(id_a, id_b);
  EXPECT_EQ(id_b, TestMap::Identifier(b));
  EXPECT_EQ(b, TestMap::Lookup(id_b));

  EXPECT_EQ(id_a, TestMap::Identifier(a));
  EXPECT_EQ(a, TestMap::Lookup(id_a));

  EXPECT_EQ(2u, TestMap::GetSizeForTesting());
}

TEST_F(WeakIdentifierMapTest, NotifyObjectDestroyed) {
  auto* a = MakeGarbageCollected<TestClass>();
  auto id_a = TestMap::Identifier(a);
  TestMap::NotifyObjectDestroyed(a);
  EXPECT_EQ(nullptr, TestMap::Lookup(id_a));

  // Simulate that an object is newly allocated at the same address.
  EXPECT_NE(id_a, TestMap::Identifier(a));
}

TEST_F(WeakIdentifierMapTest, GarbageCollected) {
  auto* a = MakeGarbageCollected<TestClass>();
  auto id_a = TestMap::Identifier(a);

  a = nullptr;
  CollectGarbage();
  EXPECT_EQ(nullptr, TestMap::Lookup(id_a));
}

TEST_F(WeakIdentifierMapTest, UnusedID) {
  auto* a = MakeGarbageCollected<TestClass>();
  auto id_a = TestMap::Identifier(a);
  EXPECT_EQ(nullptr, TestMap::Lookup(id_a + 1));
}

TEST_F(WeakIdentifierMapTest, Overflow) {
  TestMap::SetLastIdForTesting(0);
  auto* a = MakeGarbageCollected<TestClass>();
  EXPECT_EQ(1, TestMap::Identifier(a));
  EXPECT_EQ(a, TestMap::Lookup(1));

  TestMap::SetLastIdForTesting(INT_MAX - 1);
  auto* b = MakeGarbageCollected<TestClass>();
  EXPECT_EQ(INT_MAX, TestMap::Identifier(b));
  EXPECT_EQ(b, TestMap::Lookup(INT_MAX));

  auto* c = MakeGarbageCollected<TestClass>();
  EXPECT_EQ(2, TestMap::Identifier(c));
  EXPECT_EQ(c, TestMap::Lookup(2));

  DCHECK_EQ(3u, TestMap::GetSizeForTesting());
}

}  // namespace blink
