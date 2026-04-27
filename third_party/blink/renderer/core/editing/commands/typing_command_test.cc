// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/typing_command.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

#include <memory>

namespace blink {

class TypingCommandTest : public EditingTestBase {};

// Mock for ChromeClient.
class MockChromeClient : public EmptyChromeClient {
 public:
  unsigned int didUserChangeContentEditableContentCount = 0;
  // ChromeClient overrides:
  void DidUserChangeContentEditableContent(Element& element) override {
    didUserChangeContentEditableContentCount++;
  }
};

// http://crbug.com/1322746
TEST_F(TypingCommandTest, DeleteInsignificantText) {
  InsertStyleElement(
      "b { display: inline-block; width: 100px; }"
      "div { width: 100px; }");
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>"
                             "|<b><pre></pre></b> <a>abc</a>"
                             "</div>"),
      SetSelectionOptions());
  EditingState editing_state;
  TypingCommand::ForwardDeleteKeyPressed(GetDocument(), &editing_state);
  ASSERT_FALSE(editing_state.IsAborted());

  EXPECT_EQ(
      "<div contenteditable>"
      "|\u00A0<a>abc</a>"
      "</div>",
      GetSelectionTextFromBody());
}

// This is a regression test for https://crbug.com/585048
TEST_F(TypingCommandTest, insertLineBreakWithIllFormedHTML) {
  SetBodyContent("<div contenteditable></div>");

  // <input><form></form></input>
  Element* input1 = GetDocument().CreateRawElement(html_names::kInputTag);
  Element* form = GetDocument().CreateRawElement(html_names::kFormTag);
  input1->AppendChild(form);

  // <tr><input><header></header></input><rbc></rbc></tr>
  Element* tr = GetDocument().CreateRawElement(html_names::kTrTag);
  Element* input2 = GetDocument().CreateRawElement(html_names::kInputTag);
  Element* header = GetDocument().CreateRawElement(html_names::kHeaderTag);
  Element* rbc = GetDocument().CreateElementForBinding(AtomicString("rbc"));
  input2->AppendChild(header);
  tr->AppendChild(input2);
  tr->AppendChild(rbc);

  Element* div = QuerySelector("div");
  div->AppendChild(input1);
  div->AppendChild(tr);

  LocalFrame* frame = GetDocument().GetFrame();
  frame->Selection().SetSelection(SelectionInDOMTree::Builder()
                                      .Collapse(Position(form, 0))
                                      .Extend(Position(header, 0))
                                      .Build(),
                                  SetSelectionOptions());

  // Inserting line break should not crash or hit assertion.
  TypingCommand::InsertLineBreak(GetDocument());
}

// http://crbug.com/767599
TEST_F(TypingCommandTest,
       DontCrashWhenReplaceSelectionCommandLeavesBadSelection) {
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>^<h1>H1</h1>ello|</div>"),
      SetSelectionOptions());

  // This call shouldn't crash.
  TypingCommand::InsertText(
      GetDocument(), " ", 0, EditCommand::PasswordEchoBehavior::kDoNotEcho,
      TypingCommand::TextCompositionType::kTextCompositionUpdate, true);
  EXPECT_EQ("<div contenteditable><h1>\xC2\xA0|</h1></div>",
            GetSelectionTextFromBody());
}

// crbug.com/794397
TEST_F(TypingCommandTest, ForwardDeleteInvalidatesSelection) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<blockquote>^"
          "<q>"
          "<table contenteditable=\"false\"><colgroup width=\"-1\">\n</table>|"
          "</q>"
          "</blockquote>"
          "<q>\n<svg></svg></q>"),
      SetSelectionOptions());

  EditingState editing_state;
  TypingCommand::ForwardDeleteKeyPressed(GetDocument(), &editing_state);

  EXPECT_EQ(
      "<blockquote>"
      "<q>|<br></q>"
      "</blockquote>"
      "<q>\n<svg></svg></q>",
      GetSelectionTextFromBody());
}

// crbug.com/1382250
TEST_F(TypingCommandTest, ForwardDeleteAtTableEnd) {
  SetBodyContent("<table contenteditable></table>");
  Element* table = QuerySelector("table");
  table->setTextContent("a");
  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(table->firstChild(), 1))
                               .Build(),
                           SetSelectionOptions());

  // Should not crash.
  EditingState editing_state;
  TypingCommand::ForwardDeleteKeyPressed(GetDocument(), &editing_state);

  EXPECT_EQ("<table contenteditable>a|</table>", GetSelectionTextFromBody());
}

TEST_F(TypingCommandTest, TypedCharactersInContentEditable) {
  SetBodyContent("<table contenteditable></table>");
  Element* table = QuerySelector("table");
  table->setTextContent("a");
  MockChromeClient* chrome_client = MakeGarbageCollected<MockChromeClient>();
  table->GetDocument().GetPage()->SetChromeClientForTesting(chrome_client);
  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(table->firstChild(), 1))
                               .Build(),
                           SetSelectionOptions());
  TypingCommand::InsertText(
      GetDocument(), "b", 0, EditCommand::PasswordEchoBehavior::kDoNotEcho,
      TypingCommand::TextCompositionType::kTextCompositionUpdate, true);
  TypingCommand::InsertText(
      GetDocument(), "c", 0, EditCommand::PasswordEchoBehavior::kDoNotEcho,
      TypingCommand::TextCompositionType::kTextCompositionUpdate, true);
  EXPECT_EQ("<table contenteditable>abc|</table>", GetSelectionTextFromBody());
  EXPECT_EQ(2u, chrome_client->didUserChangeContentEditableContentCount);
}

TEST_F(TypingCommandTest, FirstTypedCharactersInContentEditable) {
  SetBodyContent("<table contenteditable></table>");
  Element* table = QuerySelector("table");
  table->setTextContent("a");
  MockChromeClient* chrome_client = MakeGarbageCollected<MockChromeClient>();
  table->GetDocument().GetPage()->SetChromeClientForTesting(chrome_client);
  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(table->firstChild(), 1))
                               .Build(),
                           SetSelectionOptions());
  EXPECT_EQ(0u, chrome_client->didUserChangeContentEditableContentCount);
  TypingCommand::InsertText(
      GetDocument(), "b", 0, EditCommand::PasswordEchoBehavior::kDoNotEcho,
      TypingCommand::TextCompositionType::kTextCompositionUpdate, true);
  EXPECT_EQ("<table contenteditable>ab|</table>", GetSelectionTextFromBody());
  EXPECT_EQ(1u, chrome_client->didUserChangeContentEditableContentCount);
}

// Tests that insertText with CRLF (\r\n) produces the same DOM
// as with LF (\n).
TEST_F(TypingCommandTest, InsertTextCrLfMatchesLf) {
  // First: insert with LF to establish baseline.
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>|</div>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "foo\nbar",
                            ASSERT_NO_EXCEPTION);
  std::string lf_result = GetSelectionTextFromBody();

  // Second: insert with CRLF — should produce identical result.
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>|</div>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "foo\r\nbar",
                            ASSERT_NO_EXCEPTION);
  std::string crlf_result = GetSelectionTextFromBody();

  EXPECT_EQ(lf_result, crlf_result);
}

TEST_F(TypingCommandTest, InsertTextCrLfNoCarriageReturnInDom) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable style='white-space:pre-wrap'>|</div>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "foo\r\nbar",
                            ASSERT_NO_EXCEPTION);

  // The \r should not appear in the resulting DOM.
  std::string result = GetSelectionTextFromBody();
  EXPECT_EQ(result.find('\r'), std::string::npos);
}

TEST_F(TypingCommandTest, InsertTextStandaloneCrNoCarriageReturnInDom) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<div contenteditable style='white-space:pre-wrap'>|</div>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "foo\rbar",
                            ASSERT_NO_EXCEPTION);

  // Standalone \r should also be normalized; no \r in the DOM.
  std::string result = GetSelectionTextFromBody();
  EXPECT_EQ(result.find('\r'), std::string::npos);
}

TEST_F(TypingCommandTest, InsertTextMixedLineEndingsMatchLf) {
  // Baseline with all LF.
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>|</div>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "a\nb\nc",
                            ASSERT_NO_EXCEPTION);
  std::string lf_result = GetSelectionTextFromBody();

  // Mixed CRLF and LF — should produce identical result.
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>|</div>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "a\r\nb\nc",
                            ASSERT_NO_EXCEPTION);
  std::string mixed_result = GetSelectionTextFromBody();

  EXPECT_EQ(lf_result, mixed_result);
}

// Ensure undo works correctly after CRLF normalization
TEST_F(TypingCommandTest, InsertTextCrLfUndo) {
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>|</div>"),
      SetSelectionOptions());

  GetDocument().execCommand("insertText", false, "foo\r\nbar",
                            ASSERT_NO_EXCEPTION);
  // Verify text was inserted
  EXPECT_NE(GetSelectionTextFromBody().find("foo"), std::string::npos);
  EXPECT_NE(GetSelectionTextFromBody().find("bar"), std::string::npos);

  // Undo should remove the inserted text
  GetDocument().execCommand("undo", false, "", ASSERT_NO_EXCEPTION);
  EXPECT_EQ("<div contenteditable>|</div>", GetSelectionTextFromBody());
}

}  // namespace blink
