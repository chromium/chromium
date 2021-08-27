// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/collection_support/heap_linked_queue.h"
#include "third_party/blink/renderer/platform/heap/heap_test_objects.h"
#include "third_party/blink/renderer/platform/heap/heap_test_utilities.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

namespace {
class HeapLinkedQueueTest : public TestSupportingGC {};
}  // namespace

TEST_F(HeapLinkedQueueTest, PushPop) {
  using Queue = HeapLinkedQueue<Member<IntegerObject>>;

  ClearOutOldGarbage();
  IntegerObject::destructor_calls = 0;

  Queue* queue = MakeGarbageCollected<Queue>();

  constexpr wtf_size_t kQueueSize = 10;

  for (wtf_size_t i = 0; i < kQueueSize; ++i)
    queue->push_back(MakeGarbageCollected<IntegerObject>(i));

  ConservativelyCollectGarbage();
  EXPECT_EQ(0, IntegerObject::destructor_calls);
  EXPECT_EQ(kQueueSize, queue->size());
  for (wtf_size_t i = 0; i < kQueueSize; ++i) {
    EXPECT_EQ(i, static_cast<size_t>(queue->front()->Value()));
    queue->pop_front();
  }
  EXPECT_TRUE(queue->IsEmpty());

  Persistent<Queue> holder = queue;

  PreciselyCollectGarbage();
  EXPECT_EQ(kQueueSize, static_cast<size_t>(IntegerObject::destructor_calls));
  EXPECT_EQ(0u, holder->size());
}

}  // namespace blink
