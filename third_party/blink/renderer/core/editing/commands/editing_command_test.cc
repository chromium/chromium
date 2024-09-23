// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/static_range.h"
#include "third_party/blink/renderer/core/editing/commands/editing_command_type.h"
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
  EditingCommandType type;
};

const CommandNameEntry kCommandNameEntries[] = {
#define V(name) {#name, EditingCommandType::k##name},
    FOR_EACH_BLINK_EDITING_COMMAND_NAME(V)
#undef V
};
// Test all commands except EditingCommandType::Invalid.
static_assert(
    std::size(kCommandNameEntries) + 1 ==
        static_cast<size_t>(EditingCommandType::kNumberOfCommandTypes),
    "must test all valid EditingCommandType");

}  // anonymous namespace

class EditingCommandTest : public EditingTestBase {};

TEST_F(EditingCommandTest, EditorCommandOrder) {
  for (size_t i = 1; i < std::size(kCommandNameEntries); ++i) {
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
        dummy_editor.CreateCommand(String(entry.name).LowerASCII());
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
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  EXPECT_TRUE(command.IsEnabled());
  div->removeAttribute(html_names::kContenteditableAttr);
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
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  EXPECT_FALSE(command.IsEnabled());
  editor.SetMark();
  EXPECT_TRUE(command.IsEnabled());
  div->removeAttribute(html_names::kContenteditableAttr);
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
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  EXPECT_TRUE(command.IsEnabled());
  div->removeAttribute(html_names::kContenteditableAttr);
  EXPECT_FALSE(command.IsEnabled());
}

TEST_F(EditingCommandTest, DeleteSoftLineBackwardTargetRanges) {
  Editor& editor = GetDocument().GetFrame()->GetEditor();
  const EditorCommand command = editor.CreateCommand("DeleteToBeginningOfLine");

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>abcdef<br>123|<div>"),
      SetSelectionOptions());
  Element* div = GetDocument().QuerySelector(AtomicString("div"));
  GetDocument().SetFocusedElement(
      div, FocusParams(SelectionBehaviorOnFocus::kNone,
                       mojom::blink::FocusType::kNone, nullptr));
  EXPECT_TRUE(command.IsEnabled());
  const StaticRangeVector* ranges = command.GetTargetRanges();
  EXPECT_EQ(1u, ranges->size());
  const StaticRange& range = *ranges->at(0);
  EXPECT_EQ("123", range.startContainer()->textContent());
  EXPECT_EQ(0u, range.startOffset());
  EXPECT_EQ(range.startContainer(), range.endContainer());
  EXPECT_EQ(3u, range.endOffset());
}

}  // namespace blink
