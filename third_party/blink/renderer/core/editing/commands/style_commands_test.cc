// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/style_commands.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/commands/typing_command.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

class StyleCommandsTest : public EditingTestBase {};

// http://crbug.com/1348478
TEST_F(StyleCommandsTest, ComputeAndSetTypingStyleWithNullPosition) {
  GetDocument().setDesignMode("on");
  InsertStyleElement(
      "b {"
      "display: inline-block;"
      "overflow-x: scroll;"
      "}");
  Selection().SetSelection(SetSelectionTextToBody("|<b></b>&#32;"),
                           SetSelectionOptions());

  EXPECT_TRUE(StyleCommands::ExecuteToggleBold(GetFrame(), nullptr,
                                               EditorCommandSource::kDOM, ""));

  EXPECT_EQ("|<b></b> ", GetSelectionTextFromBody());
}

// Regression test for strikethrough toggle failing after deleting content.
// When styled text is deleted and the decoration is toggled in the now-empty
// element, the typing style should be completely cleared to prevent the
// decoration from being applied to subsequently typed text.
TEST_F(StyleCommandsTest, StrikethroughToggleAfterDeletingContent) {
  SetBodyContent("<body contenteditable><div contenteditable>E</div></body>");
  Element* div = QuerySelector("div");
  ASSERT_TRUE(div);

  // Select the 'E' text.
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(div->firstChild(), 0))
                               .Extend(Position(div->firstChild(), 1))
                               .Build(),
                           SetSelectionOptions());

  // Apply strikethrough.
  EXPECT_TRUE(StyleCommands::ExecuteStrikethrough(
      GetFrame(), nullptr, EditorCommandSource::kDOM, ""));

  String html = div->GetInnerHTMLString();
  EXPECT_EQ("<strike>E</strike>", html);

  // Type replacement content - it should inherit strikethrough.
  TypingCommand::InsertText(
      GetDocument(), "X", 0,
      TypingCommand::TextCompositionType::kTextCompositionNone, false);

  html = div->GetInnerHTMLString();
  EXPECT_EQ("<strike>X</strike>", html);

  // Select all content in the div.
  Selection().SetSelection(
      SelectionInDOMTree::Builder().SelectAllChildren(*div).Build(),
      SetSelectionOptions());

  // Delete the content.
  TypingCommand::DeleteKeyPressed(GetDocument(), TypingCommand::kSmartDelete);

  // Verify the div is now empty.
  UpdateAllLifecyclePhasesForTest();
  EXPECT_EQ("<div contenteditable>|<br></div>", GetSelectionTextFromBody());

  // Toggle strikethrough off in the empty element.
  EXPECT_TRUE(StyleCommands::ExecuteStrikethrough(
      GetFrame(), nullptr, EditorCommandSource::kDOM, ""));

  // Type new content.
  TypingCommand::InsertText(
      GetDocument(), "Y", 0,
      TypingCommand::TextCompositionType::kTextCompositionNone, false);

  // Verify the new content doesn't have strikethrough styling.
  html = div->GetInnerHTMLString();
  EXPECT_EQ("Y", html);
}

// Regression test for strikethrough toggle in empty contenteditable.
// Verifies that toggling strikethrough off in an empty element clears the
// typing style completely.
TEST_F(StyleCommandsTest, StrikethroughToggleInEmptyContentEditable) {
  SetBodyContent("<body contenteditable><div contenteditable>E</div></body>");
  Element* div = QuerySelector("div");
  ASSERT_TRUE(div);

  // Select the 'E' text.
  Selection().SetSelection(
      SelectionInDOMTree::Builder().SelectAllChildren(*div).Build(),
      SetSelectionOptions());

  // Apply strikethrough.
  EXPECT_TRUE(StyleCommands::ExecuteStrikethrough(
      GetFrame(), nullptr, EditorCommandSource::kDOM, ""));

  String html = div->GetInnerHTMLString();
  EXPECT_EQ("<strike>E</strike>", html);

  // Delete the 'E'.
  TypingCommand::DeleteKeyPressed(GetDocument(), 0);

  // The div should now be empty.
  UpdateAllLifecyclePhasesForTest();

  // Toggle strikethrough off in the empty element.
  EXPECT_TRUE(StyleCommands::ExecuteStrikethrough(
      GetFrame(), nullptr, EditorCommandSource::kDOM, ""));

  // Insert new text.
  TypingCommand::InsertText(
      GetDocument(), "N", 0,
      TypingCommand::TextCompositionType::kTextCompositionNone, false);

  // Verify the new text doesn't have strikethrough.
  html = div->GetInnerHTMLString();
  EXPECT_EQ("N", html);
}

}  // namespace blink
