// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"

#include <initializer_list>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_test_helpers.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

TEST(CustomElementReactionStackTest, one) {
  Vector<char> log;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>();
  stack->Push();
  HeapVector<Member<Command>>* commands =
      MakeGarbageCollected<HeapVector<Member<Command>>>();
  commands->push_back(MakeGarbageCollected<Log>('a', log));
  stack->EnqueueToCurrentQueue(*CreateElement("a"),
                               *MakeGarbageCollected<TestReaction>(commands));
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'a'}))
      << "popping the reaction stack should run reactions";
}

TEST(CustomElementReactionStackTest, multipleElements) {
  Vector<char> log;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>();
  stack->Push();
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('a', log));
    stack->EnqueueToCurrentQueue(*CreateElement("a"),
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('b', log));
    stack->EnqueueToCurrentQueue(*CreateElement("a"),
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'a', 'b'}))
      << "reactions should run in the order the elements queued";
}

TEST(CustomElementReactionStackTest, popTopEmpty) {
  Vector<char> log;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>();
  stack->Push();
  HeapVector<Member<Command>>* commands =
      MakeGarbageCollected<HeapVector<Member<Command>>>();
  commands->push_back(MakeGarbageCollected<Log>('a', log));
  stack->EnqueueToCurrentQueue(*CreateElement("a"),
                               *MakeGarbageCollected<TestReaction>(commands));
  stack->Push();
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>())
      << "popping the empty top-of-stack should not run any reactions";
}

TEST(CustomElementReactionStackTest, popTop) {
  Vector<char> log;

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>();
  stack->Push();
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('a', log));
    stack->EnqueueToCurrentQueue(*CreateElement("a"),
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  stack->Push();
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('b', log));
    stack->EnqueueToCurrentQueue(*CreateElement("a"),
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'b'}))
      << "popping the top-of-stack should only run top-of-stack reactions";
}

TEST(CustomElementReactionStackTest, requeueingDoesNotReorderElements) {
  Vector<char> log;

  Element& element = *CreateElement("a");

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>();
  stack->Push();
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('a', log));
    stack->EnqueueToCurrentQueue(element,
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('z', log));
    stack->EnqueueToCurrentQueue(*CreateElement("a"),
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('b', log));
    stack->EnqueueToCurrentQueue(element,
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'a', 'b', 'z'}))
      << "reactions should run together in the order elements were queued";
}

TEST(CustomElementReactionStackTest, oneReactionQueuePerElement) {
  Vector<char> log;

  Element& element = *CreateElement("a");

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>();
  stack->Push();
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('a', log));
    stack->EnqueueToCurrentQueue(element,
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('z', log));
    stack->EnqueueToCurrentQueue(*CreateElement("a"),
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  stack->Push();
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('y', log));
    stack->EnqueueToCurrentQueue(*CreateElement("a"),
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  {
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<Log>('b', log));
    stack->EnqueueToCurrentQueue(element,
                                 *MakeGarbageCollected<TestReaction>(commands));
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
  ~EnqueueToStack() override = default;
  void Trace(Visitor* visitor) override {
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

  DISALLOW_COPY_AND_ASSIGN(EnqueueToStack);
};

TEST(CustomElementReactionStackTest, enqueueFromReaction) {
  Vector<char> log;

  Element& element = *CreateElement("a");

  CustomElementReactionStack* stack =
      MakeGarbageCollected<CustomElementReactionStack>();
  stack->Push();
  {
    HeapVector<Member<Command>>* subcommands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    subcommands->push_back(MakeGarbageCollected<Log>('a', log));
    HeapVector<Member<Command>>* commands =
        MakeGarbageCollected<HeapVector<Member<Command>>>();
    commands->push_back(MakeGarbageCollected<EnqueueToStack>(
        stack, element, MakeGarbageCollected<TestReaction>(subcommands)));
    stack->EnqueueToCurrentQueue(element,
                                 *MakeGarbageCollected<TestReaction>(commands));
  }
  stack->PopInvokingReactions();

  EXPECT_EQ(log, Vector<char>({'a'})) << "enqueued reaction from another "
                                         "reaction should run in the same "
                                         "invoke";
}

}  // namespace blink
