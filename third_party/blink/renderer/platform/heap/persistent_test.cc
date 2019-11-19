// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/persistent.h"

#include <memory>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class PersistentTest : public TestSupportingGC {};

namespace {

class Receiver : public GarbageCollected<Receiver> {
 public:
  void Increment(int* counter) { ++*counter; }

  void Trace(blink::Visitor* visitor) {}
};

TEST_F(PersistentTest, BindCancellation) {
  Receiver* receiver = MakeGarbageCollected<Receiver>();
  int counter = 0;
  base::RepeatingClosure function =
      WTF::BindRepeating(&Receiver::Increment, WrapWeakPersistent(receiver),
                         WTF::Unretained(&counter));

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
      WTF::CrossThreadUnretained(&counter));

  receiver = nullptr;
  PreciselyCollectGarbage();
  std::move(function).Run();
  EXPECT_EQ(0, counter);
}

}  // namespace
}  // namespace blink
