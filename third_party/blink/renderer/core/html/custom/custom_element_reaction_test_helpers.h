// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REACTION_TEST_HELPERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REACTION_TEST_HELPERS_H_

#include "third_party/blink/renderer/core/html/custom/custom_element_reaction.h"

#include <initializer_list>
#include <memory>

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_queue.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_stack.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class Element;

class Command : public GarbageCollected<Command> {
 public:
  Command() = default;
  virtual ~Command() = default;
  virtual void Trace(Visitor* visitor) {}
  virtual void Run(Element&) = 0;

  DISALLOW_COPY_AND_ASSIGN(Command);
};

class Call : public Command {
 public:
  using Callback = base::OnceCallback<void(Element&)>;
  Call(Callback callback) : callback_(std::move(callback)) {}
  ~Call() override = default;
  void Run(Element& element) override { std::move(callback_).Run(element); }

 private:
  Callback callback_;

  DISALLOW_COPY_AND_ASSIGN(Call);
};

class Unreached : public Command {
 public:
  Unreached(const char* message) : message_(message) {}
  ~Unreached() override = default;
  void Run(Element&) override { EXPECT_TRUE(false) << message_; }

 private:
  const char* message_;

  DISALLOW_COPY_AND_ASSIGN(Unreached);
};

class Log : public Command {
 public:
  Log(char what, Vector<char>& where) : what_(what), where_(where) {}
  ~Log() override = default;
  void Run(Element&) override { where_.push_back(what_); }

 private:
  char what_;
  Vector<char>& where_;

  DISALLOW_COPY_AND_ASSIGN(Log);
};

class Recurse : public Command {
 public:
  Recurse(CustomElementReactionQueue* queue) : queue_(queue) {}
  ~Recurse() override = default;
  void Trace(Visitor* visitor) override {
    Command::Trace(visitor);
    visitor->Trace(queue_);
  }
  void Run(Element& element) override { queue_->InvokeReactions(element); }

 private:
  Member<CustomElementReactionQueue> queue_;

  DISALLOW_COPY_AND_ASSIGN(Recurse);
};

class Enqueue : public Command {
 public:
  Enqueue(CustomElementReactionQueue* queue, CustomElementReaction* reaction)
      : queue_(queue), reaction_(reaction) {}
  ~Enqueue() override = default;
  void Trace(Visitor* visitor) override {
    Command::Trace(visitor);
    visitor->Trace(queue_);
    visitor->Trace(reaction_);
  }
  void Run(Element&) override { queue_->Add(*reaction_); }

 private:
  Member<CustomElementReactionQueue> queue_;
  Member<CustomElementReaction> reaction_;

  DISALLOW_COPY_AND_ASSIGN(Enqueue);
};

class TestReaction : public CustomElementReaction {
 public:
  TestReaction(HeapVector<Member<Command>>* commands)
      : CustomElementReaction(
            *MakeGarbageCollected<TestCustomElementDefinition>(
                CustomElementDescriptor("mock-element", "mock-element"))),
        commands_(commands) {}
  ~TestReaction() override = default;
  void Trace(Visitor* visitor) override {
    CustomElementReaction::Trace(visitor);
    visitor->Trace(commands_);
  }
  void Invoke(Element& element) override {
    for (auto& command : *commands_)
      command->Run(element);
  }

 private:
  Member<HeapVector<Member<Command>>> commands_;

  DISALLOW_COPY_AND_ASSIGN(TestReaction);
};

class ResetCustomElementReactionStackForTest final {
  STACK_ALLOCATED();
 public:
  ResetCustomElementReactionStackForTest()
      : stack_(MakeGarbageCollected<CustomElementReactionStack>()),
        old_stack_(
            CustomElementReactionStackTestSupport::SetCurrentForTest(stack_)) {}

  ~ResetCustomElementReactionStackForTest() {
    CustomElementReactionStackTestSupport::SetCurrentForTest(old_stack_);
  }

  CustomElementReactionStack& Stack() { return *stack_; }

 private:
  Member<CustomElementReactionStack> stack_;
  Member<CustomElementReactionStack> old_stack_;

  DISALLOW_COPY_AND_ASSIGN(ResetCustomElementReactionStackForTest);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CUSTOM_CUSTOM_ELEMENT_REACTION_TEST_HELPERS_H_
