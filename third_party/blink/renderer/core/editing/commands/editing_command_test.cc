// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stl_util.h"
#include "third_party/blink/public/platform/web_editing_command_type.h"
#include "third_party/blink/renderer/core/editing/commands/editor_command.h"
#include "third_party/blink/renderer/core/editing/commands/editor_command_names.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

struct CommandNameEntry {
  const char* name;
  WebEditingCommandType type;
};

const CommandNameEntry kCommandNameEntries[] = {
#define V(name) {#name, WebEditingCommandType::k##name},
    FOR_EACH_BLINK_EDITING_COMMAND_NAME(V)
#undef V
};
// Test all commands except WebEditingCommandType::Invalid.
static_assert(
    base::size(kCommandNameEntries) + 1 ==
        static_cast<size_t>(WebEditingCommandType::kNumberOfCommandTypes),
    "must test all valid WebEditingCommandType");

}  // anonymous namespace

class EditingCommandTest : public EditingTestBase {};

TEST_F(EditingCommandTest, EditorCommandOrder) {
  for (size_t i = 1; i < base::size(kCommandNameEntries); ++i) {
    EXPECT_GT(0,
              WTF::CodeUnitCompareIgnoringASCIICase(
                  kCommandNameEntries[i - 1].name, kCommandNameEntries[i].name))
        << "EDITOR_COMMAND_MAP must be case-folding ordered. Incorrect index:"
        << i;
  }
}

TEST_F(EditingCommandTest, CreateCommandFromString) {
  Editor& dummy_editor = GetDocument().GetFrame()->GetEditor();
  for (const auto& entry : kCommandNameEntries) {
    const EditorCommand command = dummy_editor.CreateCommand(entry.name);
    EXPECT_EQ(static_cast<int>(entry.type), command.IdForHistogram())
        << entry.name;
  }
}

TEST_F(EditingCommandTest, CreateCommandFromStringCaseFolding) {
  Editor& dummy_editor = GetDocument().GetFrame()->GetEditor();
  for (const auto& entry : kCommandNameEntries) {
    const EditorCommand lower_name_command =
        dummy_editor.CreateCommand(String(entry.name).DeprecatedLower());
    EXPECT_EQ(static_cast<int>(entry.type), lower_name_command.IdForHistogram())
        << entry.name;
    const EditorCommand upper_name_command =
        dummy_editor.CreateCommand(String(entry.name).UpperASCII());
    EXPECT_EQ(static_cast<int>(entry.type), upper_name_command.IdForHistogram())
        << entry.name;
  }
}

TEST_F(EditingCommandTest, CreateCommandFromInvalidString) {
  const String kInvalidCommandName[] = {
      "", "iNvAlId", "12345",
  };
  Editor& dummy_editor = GetDocument().GetFrame()->GetEditor();
  for (const auto& command_name : kInvalidCommandName) {
    const EditorCommand command = dummy_editor.CreateCommand(command_name);
    EXPECT_EQ(0, command.IdForHistogram());
  }
}

TEST_F(EditingCommandTest, EnabledVisibleSelection) {
  Editor& editor = GetDocument().GetFrame()->GetEditor();
  const EditorCommand command =
      editor.CreateCommand("MoveRightAndModifySelection");
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>a|b<div>"),
      SetSelectionOptions());
  Element* div = GetDocument().QuerySelector("div");
  GetDocument().SetFocusedElement(
      div,
      FocusParams(SelectionBehaviorOnFocus::kNone, kWebFocusTypeNone, nullptr));
  EXPECT_TRUE(command.IsEnabled());
  div->removeAttribute("contenteditable");
  EXPECT_FALSE(command.IsEnabled());
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(true);
  EXPECT_TRUE(command.IsEnabled());
}

TEST_F(EditingCommandTest, EnabledVisibleSelectionAndMark) {
  Editor& editor = GetDocument().GetFrame()->GetEditor();
  const EditorCommand command = editor.CreateCommand("SelectToMark");
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>a|b<div>"),
      SetSelectionOptions());
  Element* div = GetDocument().QuerySelector("div");
  GetDocument().SetFocusedElement(
      div,
      FocusParams(SelectionBehaviorOnFocus::kNone, kWebFocusTypeNone, nullptr));
  EXPECT_FALSE(command.IsEnabled());
  editor.SetMark();
  EXPECT_TRUE(command.IsEnabled());
  div->removeAttribute("contenteditable");
  EXPECT_FALSE(command.IsEnabled());
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(true);
  EXPECT_TRUE(command.IsEnabled());
}

TEST_F(EditingCommandTest, EnabledInEditableTextOrCaretBrowsing) {
  Editor& editor = GetDocument().GetFrame()->GetEditor();
  const EditorCommand command = editor.CreateCommand("MoveRight");

  SetBodyContent("<div>abc</div>");
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(false);
  EXPECT_FALSE(command.IsEnabled());
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(true);
  EXPECT_TRUE(command.IsEnabled());

  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(false);
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>a|b<div>"),
      SetSelectionOptions());
  Element* div = GetDocument().QuerySelector("div");
  GetDocument().SetFocusedElement(
      div,
      FocusParams(SelectionBehaviorOnFocus::kNone, kWebFocusTypeNone, nullptr));
  EXPECT_TRUE(command.IsEnabled());
  div->removeAttribute("contenteditable");
  EXPECT_FALSE(command.IsEnabled());
}

}  // namespace blink
