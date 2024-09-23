// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/custom_element_definition.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/node.h"  // CustomElementState
#include "third_party/blink/renderer/core/html/custom/ce_reactions_scope.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_descriptor.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_reaction_test_helpers.h"
#include "third_party/blink/renderer/core/html/custom/custom_element_test_helpers.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

using CustomElementDefinitionTest = PageTestBase;

class ConstructorFails : public TestCustomElementDefinition {
 public:
  ConstructorFails(const CustomElementDescriptor& descriptor)
      : TestCustomElementDefinition(descriptor) {}
  ConstructorFails(const ConstructorFails&) = delete;
  ConstructorFails& operator=(const ConstructorFails&) = delete;
  ~ConstructorFails() override = default;
  bool RunConstructor(Element&) override { return false; }
};

}  // namespace

TEST_F(CustomElementDefinitionTest, upgrade_clearsReactionQueueOnFailure) {
  CustomElementTestingScope testing_scope;
  Element& element =
      *CreateElement(AtomicString("a-a")).InDocument(&GetDocument());
  EXPECT_EQ(CustomElementState::kUndefined, element.GetCustomElementState())
      << "sanity check: this element should be ready to upgrade";
  {
    CEReactionsScope reactions;
    HeapVector<Member<Command>> commands;
    commands.push_back(MakeGarbageCollected<Unreached>(
        "upgrade failure should clear the reaction queue"));
    CustomElementReactionStack& stack =
        CustomElementReactionStack::From(element.GetDocument().GetAgent());
    reactions.EnqueueToCurrentQueue(
        stack, element,
        *MakeGarbageCollected<TestReaction>(std::move(commands)));
    ConstructorFails* definition = MakeGarbageCollected<ConstructorFails>(
        CustomElementDescriptor(AtomicString("a-a"), AtomicString("a-a")));
    definition->Upgrade(element);
  }
  EXPECT_EQ(CustomElementState::kFailed, element.GetCustomElementState())
      << "failing to construct should have set the 'failed' element state";
}

TEST_F(CustomElementDefinitionTest,
       upgrade_clearsReactionQueueOnFailure_backupStack) {
  CustomElementTestingScope testing_scope;
  Element& element =
      *CreateElement(AtomicString("a-a")).InDocument(&GetDocument());
  EXPECT_EQ(CustomElementState::kUndefined, element.GetCustomElementState())
      << "sanity check: this element should be ready to upgrade";
  ResetCustomElementReactionStackForTest reset_reaction_stack(
      GetDocument().GetAgent());
  HeapVector<Member<Command>> commands;
  commands.push_back(MakeGarbageCollected<Unreached>(
      "upgrade failure should clear the reaction queue"));
  reset_reaction_stack.Stack().EnqueueToBackupQueue(
      element, *MakeGarbageCollected<TestReaction>(std::move(commands)));
  ConstructorFails* definition = MakeGarbageCollected<ConstructorFails>(
      CustomElementDescriptor(AtomicString("a-a"), AtomicString("a-a")));
  definition->Upgrade(element);
  EXPECT_EQ(CustomElementState::kFailed, element.GetCustomElementState())
      << "failing to construct should have set the 'failed' element state";
}

}  // namespace blink
