// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/set_character_data_command.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/commands/editing_state.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"

namespace blink {

class SetCharacterDataCommandTest : public EditingTestBase {};

TEST_F(SetCharacterDataCommandTest, replaceTextWithSameLength) {
  SetBodyContent("<div contenteditable>This is a good test case</div>");

  SimpleEditCommand* command = MakeGarbageCollected<SetCharacterDataCommand>(
      To<Text>(GetDocument().body()->firstChild()->firstChild()), 10, 4, "lame",
      EditCommand::PasswordEchoBehavior::kDoNotEcho);

  command->DoReapply();
  EXPECT_EQ(
      "This is a lame test case",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "This is a good test case",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, replaceTextWithLongerText) {
  SetBodyContent("<div contenteditable>This is a good test case</div>");

  SimpleEditCommand* command = MakeGarbageCollected<SetCharacterDataCommand>(
      To<Text>(GetDocument().body()->firstChild()->firstChild()), 10, 4,
      "lousy", EditCommand::PasswordEchoBehavior::kDoNotEcho);

  command->DoReapply();
  EXPECT_EQ(
      "This is a lousy test case",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "This is a good test case",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, replaceTextWithShorterText) {
  SetBodyContent("<div contenteditable>This is a good test case</div>");

  SimpleEditCommand* command = MakeGarbageCollected<SetCharacterDataCommand>(
      To<Text>(GetDocument().body()->firstChild()->firstChild()), 10, 4, "meh",
      EditCommand::PasswordEchoBehavior::kDoNotEcho);

  command->DoReapply();
  EXPECT_EQ(
      "This is a meh test case",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "This is a good test case",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, insertTextIntoEmptyNode) {
  SetBodyContent("<div contenteditable />");

  GetDocument().body()->firstChild()->appendChild(
      GetDocument().CreateEditingTextNode(""));

  SimpleEditCommand* command = MakeGarbageCollected<SetCharacterDataCommand>(
      To<Text>(GetDocument().body()->firstChild()->firstChild()), 0, 0, "hello",
      EditCommand::PasswordEchoBehavior::kDoNotEcho);

  command->DoReapply();
  EXPECT_EQ(
      "hello",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, insertTextAtEndOfNonEmptyNode) {
  SetBodyContent("<div contenteditable>Hello</div>");

  SimpleEditCommand* command = MakeGarbageCollected<SetCharacterDataCommand>(
      To<Text>(GetDocument().body()->firstChild()->firstChild()), 5, 0,
      ", world!", EditCommand::PasswordEchoBehavior::kDoNotEcho);

  command->DoReapply();
  EXPECT_EQ(
      "Hello, world!",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "Hello",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, replaceEntireNode) {
  SetBodyContent("<div contenteditable>Hello</div>");

  SimpleEditCommand* command = MakeGarbageCollected<SetCharacterDataCommand>(
      To<Text>(GetDocument().body()->firstChild()->firstChild()), 0, 5, "Bye",
      EditCommand::PasswordEchoBehavior::kDoNotEcho);

  command->DoReapply();
  EXPECT_EQ(
      "Bye",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "Hello",
      To<Text>(GetDocument().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, CombinedText) {
  InsertStyleElement(
      "#sample {"
      "text-combine-upright: all;"
      "writing-mode:vertical-lr;"
      "}");
  SetBodyContent("<div contenteditable id=sample></div>");

  const auto& sample_layout_object =
      *To<LayoutBlockFlow>(GetElementById("sample")->GetLayoutObject());
  auto* text_node = To<Text>(GetDocument().body()->firstChild()->appendChild(
      GetDocument().CreateEditingTextNode("")));
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(text_node->GetLayoutObject());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="sample" (editable)
  +--LayoutTextCombine (anonymous)
  |  +--LayoutText #text ""
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));

  SimpleEditCommand* command = MakeGarbageCollected<SetCharacterDataCommand>(
      text_node, 0, 0, "text", EditCommand::PasswordEchoBehavior::kDoNotEcho);
  command->DoReapply();
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(text_node->GetLayoutObject());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="sample" (editable)
  +--LayoutTextCombine (anonymous)
  |  +--LayoutText #text "text"
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));

  command->DoUnapply();
  UpdateAllLifecyclePhasesForTest();

  ASSERT_TRUE(text_node->GetLayoutObject());
  EXPECT_EQ(R"DUMP(
LayoutBlockFlow DIV id="sample" (editable)
  +--LayoutTextCombine (anonymous)
  |  +--LayoutText #text ""
)DUMP",
            ToSimpleLayoutTree(sample_layout_object));
}

TEST_F(SetCharacterDataCommandTest, ShouldEchoPassword) {
  SetBodyContent("<div contenteditable></div>");
  Text* text_node = GetDocument().CreateEditingTextNode("");
  GetDocument().body()->firstChild()->appendChild(text_node);

  GetDocument().GetSettings()->SetPasswordEchoEnabledPhysical(true);
  auto* physical_cmd = MakeGarbageCollected<SetCharacterDataCommand>(
      text_node, 0, 0, "hello",
      EditCommand::PasswordEchoBehavior::kEchoIfPasswordEchoPhysicalEnabled);
  EXPECT_TRUE(physical_cmd->ShouldEchoPassword());

  GetDocument().GetSettings()->SetPasswordEchoEnabledPhysical(false);
  EXPECT_FALSE(physical_cmd->ShouldEchoPassword());

  GetDocument().GetSettings()->SetPasswordEchoEnabledTouch(true);
  auto* touch_cmd = MakeGarbageCollected<SetCharacterDataCommand>(
      text_node, 0, 0, "hello",
      EditCommand::PasswordEchoBehavior::kEchoIfPasswordEchoTouchEnabled);
  EXPECT_TRUE(touch_cmd->ShouldEchoPassword());

  GetDocument().GetSettings()->SetPasswordEchoEnabledTouch(false);
  EXPECT_FALSE(touch_cmd->ShouldEchoPassword());

  auto* no_echo_cmd = MakeGarbageCollected<SetCharacterDataCommand>(
      text_node, 0, 0, "hello", EditCommand::PasswordEchoBehavior::kDoNotEcho);
  EXPECT_FALSE(no_echo_cmd->ShouldEchoPassword());
}

}  // namespace blink
