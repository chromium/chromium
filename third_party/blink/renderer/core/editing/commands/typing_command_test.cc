// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/typing_command.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/functional/function_ref.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/editing/plain_text_range.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/set_selection_options.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/loader/empty_clients.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

namespace {

// U+20B9F, which is a surrogate pair in UTF-16.
constexpr char kSurrogatePair[] = "\xF0\xA0\xAE\x9F";

// Describes `position` exactly: the index in its parent of its anchor node and
// of each of the node's ancestors, its anchor type and its offset. Positions in
// two identically built DOMs have the same description only if they're the
// same position.
std::string DescribePosition(const Position& position) {
  std::string path;
  for (const Node* node = position.AnchorNode(); node;
       node = node->parentNode()) {
    path = base::StrCat({base::NumberToString(node->NodeIndex()), "/", path});
  }
  return base::StrCat(
      {path, " ", position.ToAnchorTypeAndOffsetString().Utf8()});
}

}  // namespace

class TypingCommandTest : public EditingTestBase {
 protected:
  // Checks that with FastTypingCommandCreateSelection, an incremental
  // insertion, which IME composition updates use, leaves the caret at exactly
  // the same position as without it, for a caret at every offset and every
  // selection of up to two characters in the editable that `set_up` sets up
  // and returns, with FastPlainTextRangeCreateRange both disabled and enabled.
  // `set_up` is called before every insertion, as insertions change the DOM.
  void ExpectSameCaretsAsLegacy(
      base::FunctionRef<const ContainerNode&()> set_up) {
    const wtf_size_t length =
        TextIterator::RangeLength(EphemeralRange::RangeOfContents(set_up()));
    for (const bool fast_plain_text_range : {false, true}) {
      ScopedFastPlainTextRangeCreateRangeForTest scoped_plain_text_range(
          fast_plain_text_range);
      for (wtf_size_t start = 0; start <= length; ++start) {
        for (wtf_size_t end = start; end <= std::min(start + 2, length);
             ++end) {
          SCOPED_TRACE(testing::Message()
                       << "fast_plain_text_range=" << fast_plain_text_range
                       << " start=" << start << " end=" << end);
          std::string legacy;
          {
            ScopedFastTypingCommandCreateSelectionForTest legacy_feature(false);
            legacy = InsertIncrementally(set_up(), start, end);
          }
          const std::string fast = InsertIncrementally(set_up(), start, end);
          EXPECT_EQ(legacy, fast);
          // Make sure the comparison can't pass just because neither insertion
          // inserted anything.
          if (!fast.empty()) {
            EXPECT_NE(fast.find("xy"), std::string::npos) << fast;
          }
        }
      }
    }
  }

  void ExpectSameCaretsAsLegacyInContentEditable(const std::string& html) {
    SCOPED_TRACE(html);
    ExpectSameCaretsAsLegacy([&]() -> const ContainerNode& {
      SetBodyContent(
          base::StrCat({"<div contenteditable id='sample'>", html, "</div>"}));
      return *GetElementById("sample");
    });
  }

  // `html` has a text control with the id "sample".
  void ExpectSameCaretsAsLegacyInTextControl(const std::string& html) {
    SCOPED_TRACE(html);
    ExpectSameCaretsAsLegacy([&]() -> const ContainerNode& {
      SetBodyContent(html);
      return *To<TextControlElement>(GetElementById("sample"))
                  ->InnerEditorElement();
    });
  }

  // Sets up a contenteditable with the id "sample" that contains a shadow host.
  // The host's `mode` shadow root has text and line breaks around a slot for
  // the host's text. Returns the contenteditable.
  const Element& SetContentEditableWithShadowHost(ShadowRootMode mode) {
    SetBodyContent(
        "<div contenteditable id='sample'>ab<span id='host'>light</span>cd"
        "</div>");
    GetElementById("host")
        ->AttachShadowRootForTesting(mode)
        .SetInnerHTMLWithoutTrustedTypes("wx<br><slot></slot><br>yz");
    UpdateAllLifecyclePhasesForTest();
    return *GetElementById("sample");
  }

  // Sets up a contenteditable containing `html` in a `mode` shadow root, as in
  // a custom element. Returns the contenteditable.
  const Element& SetContentEditableInShadowTree(ShadowRootMode mode,
                                                const std::string& html) {
    SetBodyContent("<div id='host'></div>");
    ShadowRoot& shadow_root =
        GetElementById("host")->AttachShadowRootForTesting(mode);
    shadow_root.SetInnerHTMLWithoutTrustedTypes(
        String::FromUtf8(std::string_view(base::StrCat(
            {"<div contenteditable id='sample'>", html, "</div>"}))));
    UpdateAllLifecyclePhasesForTest();
    return *shadow_root.getElementById(AtomicString("sample"));
  }

 private:
  // Selects the plain text offsets [start, end) of `scope` and replaces the
  // selected text incrementally with itself followed by "xy", as an IME
  // composition update that adds characters does. Returns the resulting flat
  // tree, which includes shadow trees and the inner editors of text controls,
  // with the selection marked, followed by exact descriptions of the
  // selection's anchor and focus. Returns an empty string without inserting if
  // the selection spans a line break, including one in a shadow tree:
  // compositions don't span line breaks, and replacing such text merges
  // paragraphs.
  std::string InsertIncrementally(const ContainerNode& scope,
                                  wtf_size_t start,
                                  wtf_size_t end) {
    const EphemeralRange range = PlainTextRange(start, end).CreateRange(scope);
    const String selected_text = PlainText(
        range, TextIteratorBehavior::EmitsObjectReplacementCharacterBehavior());
    if (selected_text.contains('\n') ||
        PlainText(ToEphemeralRangeInFlatTree(range)).contains('\n')) {
      return std::string();
    }
    Selection().SetSelection(
        SelectionInDomTree::Builder().SetBaseAndExtent(range).Build(),
        SetSelectionOptions::Builder().SetShouldCloseTyping(true).Build());
    TypingCommand::InsertText(
        GetDocument(), selected_text + "xy", 0,
        EditCommand::PasswordEchoBehavior::kDoNotEcho,
        TypingCommand::TextCompositionType::kTextCompositionUpdate,
        /*is_incremental_insertion=*/true);
    const SelectionInDomTree& selection = Selection().GetSelectionInDomTree();
    EXPECT_TRUE(selection.IsCaret());
    return base::StrCat({GetSelectionTextInFlatTreeFromBody(
                             ConvertToSelectionInFlatTree(selection)),
                         " anchor: ", DescribePosition(selection.Anchor()),
                         " focus: ", DescribePosition(selection.Focus())});
  }

  // Turns FastTypingCommandCreateSelection on for every test, so that no test
  // depends on the flag's default. ExpectSameCaretsAsLegacy() turns it off to
  // make the legacy insertions.
  ScopedFastTypingCommandCreateSelectionForTest scoped_feature_{true};
};

// Mock for ChromeClient.
class MockChromeClient : public EmptyChromeClient {
 public:
  size_t did_user_change_content_editable_content_count = 0;
  // ChromeClient overrides:
  void DidUserChangeContentEditableContent(Element& element) override {
    ++did_user_change_content_editable_content_count;
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
  frame->Selection().SetSelection(SelectionInDomTree::Builder()
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
  Selection().SetSelection(SelectionInDomTree::Builder()
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
  Selection().SetSelection(SelectionInDomTree::Builder()
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
  EXPECT_EQ(2u, chrome_client->did_user_change_content_editable_content_count);
}

TEST_F(TypingCommandTest, FirstTypedCharactersInContentEditable) {
  SetBodyContent("<table contenteditable></table>");
  Element* table = QuerySelector("table");
  table->setTextContent("a");
  MockChromeClient* chrome_client = MakeGarbageCollected<MockChromeClient>();
  table->GetDocument().GetPage()->SetChromeClientForTesting(chrome_client);
  UpdateAllLifecyclePhasesForTest();
  Selection().SetSelection(SelectionInDomTree::Builder()
                               .Collapse(Position(table->firstChild(), 1))
                               .Build(),
                           SetSelectionOptions());
  EXPECT_EQ(0u, chrome_client->did_user_change_content_editable_content_count);
  TypingCommand::InsertText(
      GetDocument(), "b", 0, EditCommand::PasswordEchoBehavior::kDoNotEcho,
      TypingCommand::TextCompositionType::kTextCompositionUpdate, true);
  EXPECT_EQ("<table contenteditable>ab|</table>", GetSelectionTextFromBody());
  EXPECT_EQ(1u, chrome_client->did_user_change_content_editable_content_count);
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

// An incremental insertion puts the caret after the inserted text by converting
// its plain text offset to a position, which FastTypingCommandCreateSelection
// does without computing the positions of the runs before it, like the
// newlines emitted for <br>s.
TEST_F(TypingCommandTest, IncrementalInsertionInContentEditableMatchesLegacy) {
  // Line breaks, including at the start and the end.
  ExpectSameCaretsAsLegacyInContentEditable("<br>ab<br><br>cd<br>");
  // Blocks, whose separators are emitted only after text.
  ExpectSameCaretsAsLegacyInContentEditable("<p>first</p><p>second</p>");
  ExpectSameCaretsAsLegacyInContentEditable(
      "<div>first</div><div>second</div>");
  // A replaced element, an inline element and a table.
  ExpectSameCaretsAsLegacyInContentEditable(
      "ab<img width='10' height='10'>cd<b>ef</b>"
      "<table><tr><td>g</td><td>h</td></tr></table>ij");
}

// Text controls don't use incremental insertions, which are only for richly
// editable content, but a <textarea>'s inner editor has a <br> for every line
// break, like the contenteditables that FastTypingCommandCreateSelection speeds
// up. This doesn't check empty text controls, as an incremental insertion into
// an empty inner editor fails a DCHECK in InsertIncrementalTextCommand before
// it gets to CreateCollapsedSelection().
TEST_F(TypingCommandTest, IncrementalInsertionInTextControlMatchesLegacy) {
  ExpectSameCaretsAsLegacyInTextControl(
      "<textarea id='sample'>ab\n\ncd\nef  gh\n\n</textarea>");
  ExpectSameCaretsAsLegacyInTextControl(
      base::StrCat({"<textarea id='sample'>", kSurrogatePair, "a",
                    kSurrogatePair, "\nb", kSurrogatePair, "</textarea>"}));
  ExpectSameCaretsAsLegacyInTextControl(
      "<input id='sample' value='hello   world'>");
  ExpectSameCaretsAsLegacyInTextControl(
      base::StrCat({"<input id='sample' value='a", kSurrogatePair, "b'>"}));
}

// CreateCollapsedSelection() converts offsets with
// PlainTextRange::CreateRange(), whose text iterator doesn't enter shadow
// roots, open or closed, so the runs before the caret include a shadow host's
// slotted text, but not its shadow text and line breaks.
TEST_F(TypingCommandTest, IncrementalInsertionWithShadowHostMatchesLegacy) {
  for (const ShadowRootMode mode :
       {ShadowRootMode::kOpen, ShadowRootMode::kClosed}) {
    SCOPED_TRACE(mode == ShadowRootMode::kOpen ? "open" : "closed");
    ExpectSameCaretsAsLegacy([&]() -> const ContainerNode& {
      return SetContentEditableWithShadowHost(mode);
    });
  }
}

TEST_F(TypingCommandTest, IncrementalInsertionInShadowTreeMatchesLegacy) {
  for (const ShadowRootMode mode :
       {ShadowRootMode::kOpen, ShadowRootMode::kClosed}) {
    SCOPED_TRACE(mode == ShadowRootMode::kOpen ? "open" : "closed");
    ExpectSameCaretsAsLegacy([&]() -> const ContainerNode& {
      return SetContentEditableInShadowTree(mode, "<br>ab<br><br>cd<br>");
    });
  }
}

// Collapsed whitespace isn't part of the plain text, so plain text offsets
// differ from DOM offsets.
TEST_F(TypingCommandTest,
       IncrementalInsertionWithCollapsibleWhitespaceMatchesLegacy) {
  ExpectSameCaretsAsLegacyInContentEditable("a   b    c");
  ExpectSameCaretsAsLegacyInContentEditable(
      "<span>ab</span><span>cd</span>   <span> ef </span>  <span>gh</span>");
  ExpectSameCaretsAsLegacyInContentEditable(
      "\n  <div>  first   line  </div>\n"
      "  <div>second <span> line </span> </div>\n");
}

// Editables with no plain text: empty, only collapsible whitespace and only
// non-displayed content.
TEST_F(TypingCommandTest, IncrementalInsertionWithoutVisibleTextMatchesLegacy) {
  ExpectSameCaretsAsLegacyInContentEditable("");
  ExpectSameCaretsAsLegacyInContentEditable("   ");
  ExpectSameCaretsAsLegacyInContentEditable(
      "<span style='display:none'>hidden</span>");
}

// Plain text offsets count UTF-16 code units, so they can point into a
// surrogate pair or between a character and its combining marks.
TEST_F(TypingCommandTest,
       IncrementalInsertionWithSurrogatePairsAndCombiningMarksMatchesLegacy) {
  ExpectSameCaretsAsLegacyInContentEditable(
      base::StrCat({"a", kSurrogatePair, "b", kSurrogatePair, kSurrogatePair,
                    "c", kSurrogatePair, "d"}));
  // U+0301 COMBINING ACUTE ACCENT and U+0308 COMBINING DIAERESIS.
  ExpectSameCaretsAsLegacyInContentEditable("ae\xCC\x81i\xCC\x88o");
}

}  // namespace blink
