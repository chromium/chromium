// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_queue.h"

#include <initializer_list>
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_test_helpers.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

TEST(CustomElementReactionQueueTest, invokeReactions_one) {
  Vector<char> log;
  CustomElementReactionQueue* queue =
      MakeGarbageCollected<CustomElementReactionQueue>();
  HeapVector<Member<Command>>* commands =
      MakeGarbageCollected<HeapVector<Member<Command>>>();
  commands->push_back(MakeGarbageCollected<Log>('a', log));
  queue->Add(*MakeGarbageCollected<TestReaction>(commands));
  Element* test_element = CreateElement(AtomicString("my-element"));
  queue->InvokeReactions(*test_element);
  EXPECT_EQ(log, Vector<char>({'a'}))
      << "the reaction should have been invoked";
}

TEST(CustomElementReactionQueueTest, invokeReactions_many) {
  Vector<char> log;
  CustomElementReactionQueue* queue =
      MakeGarbageCollected<CustomElementReactionQueue>();
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('a', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(commands));
  }
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('b', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(commands));
  }
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('c', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(commands));
  }
  Element* test_element = CreateElement(AtomicString("my-element"));
  queue->InvokeReactions(*test_element);
  EXPECT_EQ(log, Vector<char>({'a', 'b', 'c'}))
      << "the reaction should have been invoked";
}

TEST(CustomElementReactionQueueTest, invokeReactions_recursive) {
  Vector<char> log;
  CustomElementReactionQueue* queue =
      MakeGarbageCollected<CustomElementReactionQueue>();

  HeapVector<Member<Command>>* third_commands =
      MakeGarbageCollected<HeapVector<Member<Command>>>();
  third_commands->push_back(MakeGarbageCollected<Log>('c', log));
  third_commands->push_back(MakeGarbageCollected<Recurse>(queue));
  CustomElementReaction* third =
      MakeGarbageCollected<TestReaction>(third_commands);  // "Empty" recursion

  HeapVector<Member<Command>>* second_commands =
      MakeGarbageCollected<HeapVector<Member<Command>>>();
  second_commands->push_back(MakeGarbageCollected<Log>('b', log));
  second_commands->push_back(MakeGarbageCollected<Enqueue>(queue, third));
  CustomElementReaction* second = MakeGarbageCollected<TestReaction>(
      second_commands);  // Unwinds one level of recursion

  HeapVector<Member<Command>>* first_commands =
      MakeGarbageCollected<HeapVector<Member<Command>>>();
  first_commands->push_back(MakeGarbageCollected<Log>('a', log));
  first_commands->push_back(MakeGarbageCollected<Enqueue>(queue, second));
  first_commands->push_back(MakeGarbageCollected<Recurse>(queue));
  CustomElementReaction* first = MakeGarbageCollected<TestReaction>(
      first_commands);  // Non-empty recursion

  queue->Add(*first);
  Element* test_element = CreateElement(AtomicString("my-element"));
  queue->InvokeReactions(*test_element);
  EXPECT_EQ(log, Vector<char>({'a', 'b', 'c'}))
      << "the reactions should have been invoked";
}

TEST(CustomElementReactionQueueTest, clear_duringInvoke) {
  Vector<char> log;
  CustomElementReactionQueue* queue =
      MakeGarbageCollected<CustomElementReactionQueue>();

  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('a', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(commands));
  }
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Call>(WTF::Bind(
        [](CustomElementReactionQueue* queue, Element&) { queue->Clear(); },
        WrapPersistent(queue))));
    queue->Add(*MakeGarbageCollected<TestReaction>(commands));
  }
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('b', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(commands));
  }

  Element* test_element = CreateElement(AtomicString("my-element"));
  queue->InvokeReactions(*test_element);
  EXPECT_EQ(log, Vector<char>({'a'}))
      << "only 'a' should be logged; the second log should have been cleared";
}

}  // namespace blink
