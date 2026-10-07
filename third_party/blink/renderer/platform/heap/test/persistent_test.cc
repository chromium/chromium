// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/persistent.h"

#include <memory>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/cross_thread_persistent.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/safe_non_retaining_persistent.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class PersistentTest : public TestSupportingGC {};

namespace {

class Receiver : public GarbageCollected<Receiver> {
 public:
  void Increment(int* counter) { ++*counter; }

  void Trace(Visitor* visitor) const {}
};

TEST_F(PersistentTest, BindCancellation) {
  Receiver* receiver = MakeGarbageCollected<Receiver>();
  int counter = 0;
  base::RepeatingClosure function = BindRepeating(
      &Receiver::Increment, WrapWeakPersistent(receiver), Unretained(&counter));

  function.Run();
  EXPECT_EQ(1, counter);

  receiver = nullptr;
  PreciselyCollectGarbage();
  function.Run();
  EXPECT_EQ(1, counter);
}

TEST_F(PersistentTest, CrossThreadBindCancellation) {
  Receiver* receiver = MakeGarbageCollected<Receiver>();
  int counter = 0;
  CrossThreadOnceClosure function = CrossThreadBindOnce(
      &Receiver::Increment, WrapCrossThreadWeakPersistent(receiver),
      CrossThreadUnretained(&counter));

  receiver = nullptr;
  PreciselyCollectGarbage();
  std::move(function).Run();
  EXPECT_EQ(0, counter);
}

TEST_F(PersistentTest, UnretainedPersistentAccess) {
  Persistent<Receiver> owner = MakeGarbageCollected<Receiver>();
  SafeNonRetainingPersistent<Receiver> unretained(owner.Get());
  int counter = 0;

  PreciselyCollectGarbage();
  EXPECT_EQ(owner.Get(), unretained.Get());
  EXPECT_EQ(owner.Get(), &*unretained);
  Receiver* raw = unretained;
  EXPECT_EQ(owner.Get(), raw);
  unretained->Increment(&counter);
  EXPECT_EQ(1, counter);
}

TEST_F(PersistentTest, UnretainedPersistentDiesWhenNull) {
  SafeNonRetainingPersistent<Receiver> unretained(
      MakeGarbageCollected<Receiver>());
  PreciselyCollectGarbage();
  int counter = 0;
  EXPECT_CHECK_DEATH(unretained.Get());
  EXPECT_CHECK_DEATH(*unretained);
  EXPECT_CHECK_DEATH(unretained->Increment(&counter));
}

}  // namespace
}  // namespace blink
