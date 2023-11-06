// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/insert_text_command.h"

#include "build/buildflag.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"

namespace blink {

class InsertTextCommandTest : public EditingTestBase {};

// http://crbug.com/714311
TEST_F(InsertTextCommandTest, WithTypingStyle) {
  SetBodyContent("<div contenteditable=true><option id=sample></option></div>");
  Element* const sample = GetDocument().getElementById(AtomicString("sample"));
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(sample, 0)).Build(),
      SetSelectionOptions());
  // Register typing style to make |InsertTextCommand| to attempt to apply
  // style to inserted text.
  GetDocument().execCommand("fontSizeDelta", false, "+3", ASSERT_NO_EXCEPTION);
  auto* const command =
      MakeGarbageCollected<InsertTextCommand>(GetDocument(), "x");
  command->Apply();

  EXPECT_EQ(
      "<div contenteditable=\"true\"><option id=\"sample\">x</option></div>",
      GetDocument().body()->innerHTML())
      << "Content of OPTION is distributed into shadow node as text"
         "without applying typing style.";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertChar) {
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable><span>\ta|c</span></p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "B", ASSERT_NO_EXCEPTION);
  EXPECT_EQ("<p contenteditable><span>\taB|c</span></p>",
            GetSelectionTextFromBody())
      << "We should not split Text node";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertCharToWhiteSpacePre) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<p contenteditable><span style='white-space:pre'>\ta|c</span></p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "B", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<p contenteditable>"
      "<span style=\"white-space:pre\">\ta</span>"
      "B|"
      "<span style=\"white-space:pre\">c</span>"
      "</p>",
      GetSelectionTextFromBody())
      << "This is a just record current behavior. We should not split SPAN.";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertSpace) {
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable><span>\ta|c</span></p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "  ", ASSERT_NO_EXCEPTION);
  EXPECT_EQ("<p contenteditable><span>\ta\xC2\xA0 |c</span></p>",
            GetSelectionTextFromBody())
      << "We should insert U+0020 without splitting SPAN";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertSpaceToWhiteSpacePre) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<p contenteditable><span style='white-space:pre'>\ta|c</span></p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "  ", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<p contenteditable>"
      "<span style=\"white-space:pre\">\ta</span>"
      "\xC2\xA0\xC2\xA0|"
      "<span style=\"white-space:pre\">c</span></p>",
      GetSelectionTextFromBody())
      << "We should insert U+0020 without splitting SPAN";
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertTab) {
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable><span>\ta|c</span></p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "\t", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<p contenteditable>"
      "<span>\ta<span style=\"white-space:pre\">\t|</span>c</span>"
      "</p>",
      GetSelectionTextFromBody());
}

// http://crbug.com/741826
TEST_F(InsertTextCommandTest, InsertTabToWhiteSpacePre) {
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<p contenteditable><span style='white-space:pre'>\ta|c</span></p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "\t", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<p contenteditable><span style=\"white-space:pre\">\ta\t|c</span></p>",
      GetSelectionTextFromBody());
}

// http://crbug.com/752860
TEST_F(InsertTextCommandTest, WhitespaceFixupBeforeParagraph) {
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux ^bar|<p>baz</p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space after "qux" should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody());

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux^ bar|<p>baz</p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody());

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux^bar| <p>baz</p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space after "bar" was already being collapsed before the edit. It
  // should not have been converted to a no-break space.
  EXPECT_EQ("<div contenteditable>qux|<p>baz</p></div>",
            GetSelectionTextFromBody());

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux^bar |<p>baz</p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody());

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>qux\t^bar|<p>baz</p>"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The tab should have been converted to a no-break space (U+00A0) to prevent
  // it from being collapsed.
  EXPECT_EQ("<div contenteditable>qux\xC2\xA0|<p>baz</p></div>",
            GetSelectionTextFromBody());
}

TEST_F(InsertTextCommandTest, WhitespaceFixupAfterParagraph) {
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^bar| qux"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space before "qux" should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>|\xC2\xA0qux</div>",
            GetSelectionTextFromBody());

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^bar |qux"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>\xC2\xA0|qux</div>",
            GetSelectionTextFromBody());

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p> ^bar|qux"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The space before "bar" was already being collapsed before the edit. It
  // should not have been converted to a no-break space.
  EXPECT_EQ("<div contenteditable><p>baz</p>|qux</div>",
            GetSelectionTextFromBody());

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^ bar|qux"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, " ", ASSERT_NO_EXCEPTION);
  // The newly-inserted space should have been converted to a no-break space
  // (U+00A0) to prevent it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>\xC2\xA0|qux</div>",
            GetSelectionTextFromBody());

  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable><p>baz</p>^bar|\tqux"),
      SetSelectionOptions());
  GetDocument().execCommand("insertText", false, "", ASSERT_NO_EXCEPTION);
  // The tab should have been converted to a no-break space (U+00A0) to prevent
  // it from being collapsed.
  EXPECT_EQ("<div contenteditable><p>baz</p>|\xC2\xA0qux</div>",
            GetSelectionTextFromBody());
}

// http://crbug.com/779376
TEST_F(InsertTextCommandTest, NoVisibleSelectionAfterDeletingSelection) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  InsertStyleElement(
      ":root { font-size: 10px; }"
      "ruby { display: inline-block; height: 100%; }"
      "navi { float: left; }");
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable>"
                             "  <ruby><strike>"
                             "    <navi></navi>"
                             "    <rtc>^&#xbbc3;&#xff17;&#x8e99;&#x1550;</rtc>"
                             "  </strike></ruby>"
                             "  <hr>|"
                             "</div>"),
      SetSelectionOptions());
  // Shouldn't crash inside
  GetDocument().execCommand("insertText", false, "x", ASSERT_NO_EXCEPTION);
  // This is only for recording the current behavior, which can be changed.
  EXPECT_EQ(
      "<div contenteditable>"
      "  <ruby><strike>"
      "    <navi></navi>"
      "    ^</strike></ruby>"
      "|</div>",
      GetSelectionTextFromBody());
}

// http://crbug.com/778901
TEST_F(InsertTextCommandTest, CheckTabSpanElementNoCrash) {
  InsertStyleElement(
      "head {-webkit-text-stroke-color: black; display: list-item;}");
  Element* head = GetDocument().QuerySelector(AtomicString("head"));
  Element* style = GetDocument().QuerySelector(AtomicString("style"));
  Element* body = GetDocument().body();
  body->parentNode()->appendChild(style);
  GetDocument().setDesignMode("on");

  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(head, 0))
                               .Extend(Position(body, 0))
                               .Build(),
                           SetSelectionOptions());

  // Shouldn't crash inside
  GetDocument().execCommand("insertText", false, "\t", ASSERT_NO_EXCEPTION);

  // This only records the current behavior, which is not necessarily correct.
  EXPECT_EQ(
      "<body><span style=\"white-space:pre\">\t|</span></body>"
      "<style>"
      "head {-webkit-text-stroke-color: black; display: list-item;}"
      "</style>",
      SelectionSample::GetSelectionText(*GetDocument().documentElement(),
                                        Selection().GetSelectionInDOMTree()));
}

// http://crbug.com/792548
TEST_F(InsertTextCommandTest, AnchorElementWithBlockCrash) {
  GetDocument().setDesignMode("on");
  SetBodyContent("<a href=\"www\" style=\"display:block\">");
  // We need the below DOM with selection.
  // <a href=\"www\" style=\"display:block\">
  //   <a href=\"www\" style=\"display: inline !important;\">
  //   <i>^home|</i>
  //   </a>
  // </a>
  // Since the HTML parser rejects it as there are nested <a> elements.
  // We are constructing the remaining DOM manually.
  Element* const anchor = GetDocument().QuerySelector(AtomicString("a"));
  Element* nested_anchor = GetDocument().CreateRawElement(html_names::kATag);
  Element* iElement = GetDocument().CreateRawElement(html_names::kITag);

  nested_anchor->setAttribute(html_names::kHrefAttr, AtomicString("www"));
  iElement->setInnerHTML("home");

  anchor->AppendChild(nested_anchor);
  nested_anchor->AppendChild(iElement);

  Node* const iElement_text_node = iElement->firstChild();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(iElement_text_node, 0),
                            Position(iElement_text_node, 4))
          .Build(),
      SetSelectionOptions());
  // Crash happens here with when '\n' is inserted.
  GetDocument().execCommand("inserttext", false, "a\n", ASSERT_NO_EXCEPTION);
  EXPECT_EQ(
      "<a href=\"www\" style=\"display:block\"><i>a</i></a><a href=\"www\" "
      "style=\"display:block\"><i>|<br></i></a>",
      GetSelectionTextFromBody());
}

// http://crbug.com/1197977
TEST_F(InsertTextCommandTest, MultilineSelectionCrash) {
  // Force line break between A and B.
  InsertStyleElement("body { width: 1px; }");
  Selection().SetSelection(SetSelectionTextToBody("A^<span> B|</span>"),
                           SetSelectionOptions());
  GetDocument().setDesignMode("on");

  // Shouldn't crash inside.
  GetDocument().execCommand("InsertText", false, "x", ASSERT_NO_EXCEPTION);
  EXPECT_EQ("A<span>x|</span>", GetSelectionTextFromBody());
}

}  // namespace blink
