// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_queue.h"

#include <initializer_list>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_test_helpers.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

TEST(CustomElementReactionQueueTest, invokeReactions_one) {
  test::TaskEnvironment task_environment;
  CustomElementTestingScope testing_scope;
  Vector<char> log;
  CustomElementReactionQueue* queue =
      MakeGarbageCollected<CustomElementReactionQueue>();
  HeapVector<Member<Command>> commands;
  commands.push_back(MakeGarbageCollected<Log>('a', log));
  queue->Add(*MakeGarbageCollected<TestReaction>(std::move(commands)));
  Element* test_element = CreateElement(AtomicString("my-element"));
  queue->InvokeReactions(*test_element);
  EXPECT_EQ(log, Vector<char>({'a'}))
      << "the reaction should have been invoked";
}

TEST(CustomElementReactionQueueTest, invokeReactions_many) {
  test::TaskEnvironment task_environment;
  CustomElementTestingScope testing_scope;
  Vector<char> log;
  CustomElementReactionQueue* queue =
      MakeGarbageCollected<CustomElementReactionQueue>();
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('a', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('b', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('c', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  Element* test_element = CreateElement(AtomicString("my-element"));
  queue->InvokeReactions(*test_element);
  EXPECT_EQ(log, Vector<char>({'a', 'b', 'c'}))
      << "the reaction should have been invoked";
}

TEST(CustomElementReactionQueueTest, invokeReactions_recursive) {
  test::TaskEnvironment task_environment;
  CustomElementTestingScope testing_scope;
  Vector<char> log;
  CustomElementReactionQueue* queue =
      MakeGarbageCollected<CustomElementReactionQueue>();

  HeapVector<Member<Command>> third_commands;
  third_commands.push_back(MakeGarbageCollected<Log>('c', log));
  third_commands.push_back(MakeGarbageCollected<Recurse>(queue));
  CustomElementReaction* third = MakeGarbageCollected<TestReaction>(
      std::move(third_commands));  // "Empty" recursion

  HeapVector<Member<Command>> second_commands;
  second_commands.push_back(MakeGarbageCollected<Log>('b', log));
  second_commands.push_back(MakeGarbageCollected<Enqueue>(queue, third));
  CustomElementReaction* second = MakeGarbageCollected<TestReaction>(
      std::move(second_commands));  // Unwinds one level of recursion

  HeapVector<Member<Command>> first_commands;
  first_commands.push_back(MakeGarbageCollected<Log>('a', log));
  first_commands.push_back(MakeGarbageCollected<Enqueue>(queue, second));
  first_commands.push_back(MakeGarbageCollected<Recurse>(queue));
  CustomElementReaction* first = MakeGarbageCollected<TestReaction>(
      std::move(first_commands));  // Non-empty recursion

  queue->Add(*first);
  Element* test_element = CreateElement(AtomicString("my-element"));
  queue->InvokeReactions(*test_element);
  EXPECT_EQ(log, Vector<char>({'a', 'b', 'c'}))
      << "the reactions should have been invoked";
}

TEST(CustomElementReactionQueueTest, clear_duringInvoke) {
  test::TaskEnvironment task_environment;
  CustomElementTestingScope testing_scope;
  Vector<char> log;
  CustomElementReactionQueue* queue =
      MakeGarbageCollected<CustomElementReactionQueue>();

  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('a', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Call>(WTF::BindOnce(
        [](CustomElementReactionQueue* queue, Element&) { queue->Clear(); },
        WrapPersistent(queue))));
    queue->Add(*MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('b', log));
    queue->Add(*MakeGarbageCollected<TestReaction>(std::move(commands)));
  }

  Element* test_element = CreateElement(AtomicString("my-element"));
  queue->InvokeReactions(*test_element);
  EXPECT_EQ(log, Vector<char>({'a'}))
      << "only 'a' should be logged; the second log should have been cleared";
}

}  // namespace blink
