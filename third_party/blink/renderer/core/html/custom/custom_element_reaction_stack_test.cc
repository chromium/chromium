// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"

#include <initializer_list>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_test_helpers.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/core/testing/null_execution_context.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

TEST(CustomElementReactionStackTest, one) {
  test::TaskEnvironment task_environment;
  Vector<char> log;
  CustomElementTestingScope testing_scope;
  ScopedNullExecutionContext execution_context;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>(
          *execution_context.GetExecutionContext().GetAgent());
  stack->Push();
  HeapVector<Member<Command>> commands;
  commands.push_back(MakeGarbageCollected<Log>('a', log));
  stack->EnqueueToCurrentQueue(
      *CreateElement(AtomicString("a")),
      *MakeGarbageCollected<TestReaction>(std::move(commands)));
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'a'}))
      << "popping the reaction stack should run reactions";
}

TEST(CustomElementReactionStackTest, multipleElements) {
  test::TaskEnvironment task_environment;
  Vector<char> log;
  CustomElementTestingScope testing_scope;
  ScopedNullExecutionContext execution_context;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>(
          *execution_context.GetExecutionContext().GetAgent());
  stack->Push();
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('a', log));
    stack->EnqueueToCurrentQueue(
        *CreateElement(AtomicString("a")),
        *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('b', log));
    stack->EnqueueToCurrentQueue(
        *CreateElement(AtomicString("a")),
        *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'a', 'b'}))
      << "reactions should run in the order the elements queued";
}

TEST(CustomElementReactionStackTest, popTopEmpty) {
  test::TaskEnvironment task_environment;
  Vector<char> log;
  CustomElementTestingScope testing_scope;
  ScopedNullExecutionContext execution_context;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>(
          *execution_context.GetExecutionContext().GetAgent());
  stack->Push();
  HeapVector<Member<Command>> commands;
  commands.push_back(MakeGarbageCollected<Log>('a', log));
  stack->EnqueueToCurrentQueue(
      *CreateElement(AtomicString("a")),
      *MakeGarbageCollected<TestReaction>(std::move(commands)));
  stack->Push();
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>())
      << "popping the empty top-of-stack should not run any reactions";
}

TEST(CustomElementReactionStackTest, popTop) {
  test::TaskEnvironment task_environment;
  Vector<char> log;
  CustomElementTestingScope testing_scope;
  ScopedNullExecutionContext execution_context;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>(
          *execution_context.GetExecutionContext().GetAgent());
  stack->Push();
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('a', log));
    stack->EnqueueToCurrentQueue(
        *CreateElement(AtomicString("a")),
        *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  stack->Push();
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('b', log));
    stack->EnqueueToCurrentQueue(
        *CreateElement(AtomicString("a")),
        *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'b'}))
      << "popping the top-of-stack should only run top-of-stack reactions";
}

TEST(CustomElementReactionStackTest, requeueingDoesNotReorderElements) {
  test::TaskEnvironment task_environment;
  Vector<char> log;
  CustomElementTestingScope testing_scope;

  Element& element = *CreateElement(AtomicString("a"));
  ScopedNullExecutionContext execution_context;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>(
          *execution_context.GetExecutionContext().GetAgent());
  stack->Push();
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('a', log));
    stack->EnqueueToCurrentQueue(
        element, *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('z', log));
    stack->EnqueueToCurrentQueue(
        *CreateElement(AtomicString("a")),
        *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('b', log));
    stack->EnqueueToCurrentQueue(
        element, *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'a', 'b', 'z'}))
      << "reactions should run together in the order elements were queued";
}

TEST(CustomElementReactionStackTest, oneReactionQueuePerElement) {
  test::TaskEnvironment task_environment;
  Vector<char> log;
  CustomElementTestingScope testing_scope;

  Element& element = *CreateElement(AtomicString("a"));

  ScopedNullExecutionContext execution_context;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>(
          *execution_context.GetExecutionContext().GetAgent());
  stack->Push();
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('a', log));
    stack->EnqueueToCurrentQueue(
        element, *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('z', log));
    stack->EnqueueToCurrentQueue(
        *CreateElement(AtomicString("a")),
        *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  stack->Push();
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('y', log));
    stack->EnqueueToCurrentQueue(
        *CreateElement(AtomicString("a")),
        *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  {
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Log>('b', log));
    stack->EnqueueToCurrentQueue(
        element, *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'y', 'a', 'b'}))
      << "reactions should run together in the order elements were queued";

  log.clear();
  stack->PopInvokingReactions();
  EXPECT_EQ(log, Vector<char>({'z'})) << "reactions should be run once";
}

class EnqueueToStack : public Command {
 public:
  EnqueueToStack(CustomElementReactionStack* stack,
                 Element& element,
                 CustomElementReaction* reaction)
      : stack_(stack), element_(element), reaction_(reaction) {}
  EnqueueToStack(const EnqueueToStack&) = delete;
  EnqueueToStack& operator=(const EnqueueToStack&) = delete;
  ~EnqueueToStack() override = default;
  void Trace(Visitor* visitor) const override {
    Command::Trace(visitor);
    visitor->Trace(stack_);
    visitor->Trace(element_);
    visitor->Trace(reaction_);
  }
  void Run(Element&) override {
    stack_->EnqueueToCurrentQueue(*element_, *reaction_);
  }

 private:
  Member<CustomElementReactionStack> stack_;
  Member<Element> element_;
  Member<CustomElementReaction> reaction_;
};

TEST(CustomElementReactionStackTest, enqueueFromReaction) {
  test::TaskEnvironment task_environment;
  Vector<char> log;
  CustomElementTestingScope testing_scope;

  Element& element = *CreateElement(AtomicString("a"));
  ScopedNullExecutionContext execution_context;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>(
          *execution_context.GetExecutionContext().GetAgent());
  stack->Push();
  {
    HeapVector<Member<Command>> subcommands;
    subcommands.push_back(MakeGarbageCollected<Log>('a', log));
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<EnqueueToStack>(
        stack, element,
        MakeGarbageCollected<TestReaction>(std::move(subcommands))));
    stack->EnqueueToCurrentQueue(
        element, *MakeGarbageCollected<TestReaction>(std::move(commands)));
  }
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'a'})) << "enqueued reaction from another "
                                         "reaction should run in the same "
                                         "invoke";
}

}  // namespace blink
