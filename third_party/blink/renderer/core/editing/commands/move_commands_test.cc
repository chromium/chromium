// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/editing/commands/move_commands.h"
#include "third_party/blink/renderer/core/editing/editing_behavior.h"
#include "third_party/blink/renderer/core/editing/editor.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"

namespace blink {

class MoveCommandsTest : public EditingTestBase {
 protected:
  void VerifyCaretBrowsingPositionAndFocusUpdate(
      const std::string& initial_selection_text,
      const char* initial_focus_element,
      bool (*execute)(LocalFrame&, Event*, EditorCommandSource, const String&),
      const std::string& final_selection_text,
      const char* final_focus_element) {
    Selection().SetSelection(SetSelectionTextToBody(initial_selection_text),
                             SetSelectionOptions());
    GetDocument().SetFocusedElement(
        GetDocument().QuerySelector(AtomicString(initial_focus_element)),
        FocusParams(SelectionBehaviorOnFocus::kNone,
                    mojom::blink::FocusType::kNone, nullptr));
    GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(true);
    execute(*GetDocument().GetFrame(), nullptr,
            EditorCommandSource::kMenuOrKeyBinding, String());
    EXPECT_EQ(final_selection_text, GetSelectionTextFromBody());
    EXPECT_EQ(GetDocument().QuerySelector(AtomicString(final_focus_element)),
              GetDocument().ActiveElement());
  }
};

// The following CaretBrowsingPositionAndFocusUpdate_Move* tests verify that the
// move commands are using UpdateFocusForCaretBrowsing to adjust caret position
// and focus while caret browsing.

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveBackward) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveBackward, "<div><a href=\"foo\">|a</a>b</div>",
      "a");
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveDown) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|b</div><div><a href=\"foo\">cd</a></div>", "body",
      MoveCommands::ExecuteMoveDown,
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_IOS)
      "<div>ab</div><div><a href=\"foo\">c|d</a></div>", "a");
#else
      // MoveDown navigates visually, placing caret at different position for
      // macOS and iOS.
      "<div>ab</div><div><a href=\"foo\">|cd</a></div>", "a");
#endif
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveForward) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\">b</a></div>", "body",
      MoveCommands::ExecuteMoveForward, "<div>a<a href=\"foo\">b|</a></div>",
      "a");
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveLeft) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveLeft, "<div><a href=\"foo\">|a</a>b</div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveParagraphBackward) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveParagraphBackward,
      "<div><a href=\"foo\">|a</a>b</div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveParagraphForward) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\">b</a></div>", "body",
      MoveCommands::ExecuteMoveParagraphForward,
      "<div>a<a href=\"foo\">b|</a></div>", "a");
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveRight) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\">b</a></div>", "body",
      MoveCommands::ExecuteMoveRight, "<div>a<a href=\"foo\">b|</a></div>",
      "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveToBeginningOfDocument) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveToBeginningOfDocument,
      "<div><a href=\"foo\">|a</a>b</div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveToBeginningOfLine) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveToBeginningOfLine,
      "<div><a href=\"foo\">|a</a>b</div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveToBeginningOfParagraph) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveToBeginningOfParagraph,
      "<div><a href=\"foo\">|a</a>b</div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveToBeginningOfSentence) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveToBeginningOfSentence,
      "<div><a href=\"foo\">|a</a>b</div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveToEndOfDocument) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\">b</a></div>", "body",
      MoveCommands::ExecuteMoveToEndOfDocument,
      "<div>a<a href=\"foo\">b|</a></div>", "a");
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveToEndOfLine) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\">b</a></div>", "body",
      MoveCommands::ExecuteMoveToEndOfLine,
      "<div>a<a href=\"foo\">b|</a></div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveToEndOfParagraph) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\">b</a></div>", "body",
      MoveCommands::ExecuteMoveToEndOfParagraph,
      "<div>a<a href=\"foo\">b|</a></div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveToEndOfSentence) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\">b</a></div>", "body",
      MoveCommands::ExecuteMoveToEndOfSentence,
      "<div>a<a href=\"foo\">b|</a></div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveToLeftEndOfLine) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveToLeftEndOfLine,
      "<div><a href=\"foo\">|a</a>b</div>", "a");
}

TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveToRightEndOfLine) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\">b</a></div>", "body",
      MoveCommands::ExecuteMoveToRightEndOfLine,
      "<div>a<a href=\"foo\">b|</a></div>", "a");
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveUp) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">ab</a></div><div>c|d</div>", "body",
      MoveCommands::ExecuteMoveUp,
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_IOS)
      "<div><a href=\"foo\">a|b</a></div><div>cd</div>", "a");
#else
      // MoveUp navigates visually, placing caret at different position for
      // macOS and iOS.
      "<div><a href=\"foo\">|ab</a></div><div>cd</div>", "a");
#endif
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveWordBackward) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveWordBackward,
      "<div><a href=\"foo\">|a</a>b</div>", "a");
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveWordForward) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\">b</a></div>", "body",
      MoveCommands::ExecuteMoveWordForward,
      "<div>a<a href=\"foo\">b|</a></div>", "a");
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveWordLeft) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a</a>|b</div>", "body",
      MoveCommands::ExecuteMoveWordLeft, "<div><a href=\"foo\">|a</a>b</div>",
      "a");
}

TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_MoveWordRight) {
  bool should_skip_spaces = GetDocument()
                                .GetFrame()
                                ->GetEditor()
                                .Behavior()
                                .ShouldSkipSpaceWhenMovingRight();
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>a|<a href=\"foo\"> b</a></div>", "body",
      MoveCommands::ExecuteMoveWordRight,
      should_skip_spaces ? "<div>a<a href=\"foo\"> |b</a></div>"
                         : "<div>a<a href=\"foo\"> b|</a></div>",
      "a");
  // MoveRight skips the beginning of the word when started after
  // end of previous word, placing caret at different position for macOS.
}

// This test verifies that focus returns to the body after browsing out of a
// focusable element.
TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_ExitingFocusableElement) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div><a href=\"foo\">a|</a>b</div>", "a", MoveCommands::ExecuteMoveRight,
      "<div><a href=\"foo\">a</a>b|</div>", "body");
}

// This test verifies that caret browsing into a focusable element does not
// move focus if inside an editable region.
TEST_F(MoveCommandsTest, CaretBrowsingPositionAndFocusUpdate_EditableElements) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div contenteditable>a|<a href=\"foo\">b</a>c</div>", "div",
      MoveCommands::ExecuteMoveRight,
      "<div contenteditable>a<a href=\"foo\">b|</a>c</div>", "div");
}

// This test verifies that another focusable element (the button element) can be
// moved into while caret browsing and gains focus, just like an anchor
// element.
TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveRightButtonElement) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>Some text to the left of the button|<button>Click "
      "Me!</button></div>",
      "body", MoveCommands::ExecuteMoveRight,
      "<div>Some text to the left of the button<button>C|lick "
      "Me!</button></div>",
      "button");
}

// This test verifies that an element with tabindex set can be moved
// into while caret browsing and gains focus, just like an anchor element.
TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveRightElementWithTabIndex) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>Some text to the left of the span|<span tabindex=\"0\">Span with "
      "tabindex set</span></div>",
      "body", MoveCommands::ExecuteMoveRight,
      "<div>Some text to the left of the span<span tabindex=\"0\">S|pan with "
      "tabindex set</span></div>",
      "span");
}

// This test verifies that an input element will be skipped when caret browsing
// and not gain focus.
TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveRightInputElement) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>Some text to the left of the input element|<input type=\"text\" "
      "value=\"This is some initial text\">Some text to the right of the input "
      "element</div>",
      "body", MoveCommands::ExecuteMoveRight,
      "<div>Some text to the left of the input element<input type=\"text\" "
      "value=\"This is some initial text\">|Some text to the right of the "
      "input element</div>",
      "body");
}

// This test verifies that a contentEditable element will be skipped when caret
// browsing and not gain focus.
TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveRightContentEditableElement) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>Some text to the left of the contentEditable element|<span "
      "contentEditable=\"true\">I am content editable</span>Some text to the "
      "right of the contentEditable element</div>",
      "body", MoveCommands::ExecuteMoveRight,
      "<div>Some text to the left of the contentEditable element<span "
      "contenteditable=\"true\">I am content editable</span>|Some text to the "
      "right of the contentEditable element</div>",
      "body");
}

// This test verifies that a textarea element will be skipped when caret
// browsing and not gain focus.
TEST_F(MoveCommandsTest,
       CaretBrowsingPositionAndFocusUpdate_MoveRightTextAreaElement) {
  VerifyCaretBrowsingPositionAndFocusUpdate(
      "<div>Some text to the left of the textarea element|<textarea>I am in a "
      "textarea</textarea>Some text to the "
      "right of the textarea element</div>",
      "body", MoveCommands::ExecuteMoveRight,
      "<div>Some text to the left of the textarea element<textarea>I am in a "
      "textarea</textarea>|Some text to the "
      "right of the textarea element</div>",
      "body");
}

// This test verifies that while caret browsing if you try to move the caret
// when it is not in focus then it jumps to the active element before moving.
TEST_F(MoveCommandsTest, CaretBrowsingSelectionUpdate) {
  Selection().SetSelection(
      SetSelectionTextToBody("<div>|a<a href=\"foo\">b</a></div>"),
      SetSelectionOptions());
  GetDocument().SetFocusedElement(
      GetDocument().QuerySelector(AtomicString("a")),
      FocusParams(SelectionBehaviorOnFocus::kNone,
                  mojom::blink::FocusType::kNone, nullptr));
  GetDocument().GetFrame()->GetSettings()->SetCaretBrowsingEnabled(true);
  MoveCommands::ExecuteMoveRight(*GetDocument().GetFrame(), nullptr,
                                 EditorCommandSource::kMenuOrKeyBinding,
                                 String());
  EXPECT_EQ("<div>a<a href=\"foo\">b|</a></div>", GetSelectionTextFromBody());
}

}  // namespace blink
