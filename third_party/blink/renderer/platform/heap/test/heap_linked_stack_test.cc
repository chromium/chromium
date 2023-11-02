// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_stack.h"
#include "third_party/blink/renderer/platform/heap/heap_test_objects.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {
class HeapLinkedStackTest : public TestSupportingGC {};
}  // namespace

TEST_F(HeapLinkedStackTest, PushPop) {
  using Stack = HeapLinkedStack<Member<IntegerObject>>;

  ClearOutOldGarbage();
  IntegerObject::destructor_calls = 0;

  Stack* stack = MakeGarbageCollected<Stack>();

  constexpr wtf_size_t kStackSize = 10;

  for (wtf_size_t i = 0; i < kStackSize; i++)
    stack->Push(MakeGarbageCollected<IntegerObject>(i));

  ConservativelyCollectGarbage();
  EXPECT_EQ(0, IntegerObject::destructor_calls);
  EXPECT_EQ(kStackSize, stack->size());
  while (!stack->IsEmpty()) {
    EXPECT_EQ(stack->size() - 1, static_cast<size_t>(stack->Peek()->Value()));
    stack->Pop();
  }

  Persistent<Stack> holder = stack;

  PreciselyCollectGarbage();
  EXPECT_EQ(kStackSize, static_cast<size_t>(IntegerObject::destructor_calls));
  EXPECT_EQ(0u, holder->size());
}

}  // namespace blink
