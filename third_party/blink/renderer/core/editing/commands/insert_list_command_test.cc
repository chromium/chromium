// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/insert_list_command.h"

#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/testing/selection_sample.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

class InsertListCommandTest : public EditingTestBase {};

TEST_F(InsertListCommandTest, ShouldCleanlyRemoveSpuriousTextNode) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
  // Needs to be editable to use InsertListCommand.
  GetDocument().setDesignMode("on");
  // Set up the condition:
  // * Selection is a range, to go down into
  //   InsertListCommand::listifyParagraph.
  // * The selection needs to have a sibling list to go down into
  //   InsertListCommand::mergeWithNeighboringLists.
  // * Should be the same type (ordered list) to go into
  //   CompositeEditCommand::mergeIdenticalElements.
  // * Should have no actual children to fail the listChildNode check
  //   in InsertListCommand::doApplyForSingleParagraph.
  // * There needs to be an extra text node to trigger its removal in
  //   CompositeEditCommand::mergeIdenticalElements.
  // The removeNode is what updates document lifecycle to VisualUpdatePending
  // and makes FrameView::needsLayout return true.
  SetBodyContent("\nd\n<ol>");
  Text* empty_text = GetDocument().createTextNode("");
  GetDocument().body()->InsertBefore(empty_text,
                                     GetDocument().body()->firstChild());
  UpdateAllLifecyclePhasesForTest();
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().body(), 0))
          .Extend(Position(GetDocument().body(), 2))
          .Build(),
      SetSelectionOptions());

  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kOrderedList);
  // This should not DCHECK.
  EXPECT_TRUE(command->Apply())
      << "The insert ordered list command should have succeeded";
  EXPECT_EQ("<ol><li>\nd\n</li></ol>", GetDocument().body()->innerHTML());
}

// Refer https://crbug.com/794356
TEST_F(InsertListCommandTest, UnlistifyParagraphCrashOnVisuallyEmptyParagraph) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody("^<dl>"
                             "<textarea style='float:left;'></textarea>"
                             "</dl>|"),
      SetSelectionOptions());
  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kUnorderedList);
  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "<dl><ul>"
      "|<textarea style=\"float:left;\"></textarea>"
      "</ul></dl>",
      GetSelectionTextFromBody());
}

TEST_F(InsertListCommandTest, UnlistifyParagraphCrashOnNonLi) {
  // Checks that InsertOrderedList does not cause a crash when the caret is in a
  // non-<li> child of a list which contains non-<li> blocks.
  GetDocument().setDesignMode("on");
  Selection().SetSelection(SetSelectionTextToBody("<ol><div>|"
                                                  "<p>foo</p><p>bar</p>"
                                                  "</div></ol>"),
                           SetSelectionOptions());
  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kOrderedList);
  // Crash happens here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ("|foo<br><ol><p>bar</p></ol>", GetSelectionTextFromBody());
}

// Refer https://crbug.com/798176
TEST_F(InsertListCommandTest, CleanupNodeSameAsDestinationNode) {
  GetDocument().setDesignMode("on");
  InsertStyleElement(
      "* { -webkit-appearance:checkbox; }"
      "br { visibility:hidden; }"
      "colgroup { -webkit-column-count:2; }");
  Selection().SetSelection(SetSelectionTextToBody("^<table><col></table>"
                                                  "<button></button>|"),
                           SetSelectionOptions());
  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kUnorderedList);
  // Crash happens here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<ul><li><table><colgroup><col>"
      "</colgroup></table></li>"
      "<li><button>|</button></li></ul><br>",
      GetSelectionTextFromBody());
}

TEST_F(InsertListCommandTest, InsertListOnEmptyHiddenElements) {
  GetDocument().setDesignMode("on");
  InsertStyleElement("br { visibility:hidden; }");
  Selection().SetSelection(SetSelectionTextToBody("^<button></button>|"),
                           SetSelectionOptions());
  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kUnorderedList);

  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ("^<button><ul><li><br></li></ul></button>|",
            GetSelectionTextFromBody());
}

// Refer https://crbug.com/797520
TEST_F(InsertListCommandTest, InsertListWithCollapsedVisibility) {
  GetDocument().setDesignMode("on");
  InsertStyleElement(
      "ul { visibility:collapse; }"
      "dl { visibility:visible; }");

  Selection().SetSelection(SetSelectionTextToBody("^<dl>a</dl>|"),
                           SetSelectionOptions());
  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kOrderedList);

  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ("^<dl><ol></ol><ul>a</ul></dl>|", GetSelectionTextFromBody());
}

// Refer https://crbug.com/1183158
TEST_F(InsertListCommandTest, UnlistifyParagraphWithNonEditable) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody("<li>a|<div contenteditable=false>b</div></li>"),
      SetSelectionOptions());
  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kUnorderedList);

  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ("<ul><li>a|<div contenteditable=\"false\">b</div></li></ul><br>",
            GetSelectionTextFromBody());
}

// Refer https://crbug.com/1188327
TEST_F(InsertListCommandTest, NestedSpansJustInsideBody) {
  InsertStyleElement("span { appearance: checkbox; }");
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody("<span><span><span>a</span></span></span>|b"),
      SetSelectionOptions());
  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kUnorderedList);

  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "<ul><li><br>a</li></ul><span><span><span>^a</span></span></span>b|",
      GetSelectionTextFromBody());
}

TEST_F(InsertListCommandTest, ListifyInputInTableCell) {
  GetDocument().setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody(
          "^<ruby><div style='display: table-cell'><input style='display: "
          "table-cell' type='file' maxlength='100'><select>|"),
      SetSelectionOptions());
  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kUnorderedList);

  // Crash happens here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<ruby><div style=\"display: "
      "table-cell\"><ul><li>^<br></li><li><ruby><div style=\"display: "
      "table-cell\">|<input maxlength=\"100\" style=\"display: table-cell\" "
      "type=\"file\"></div></ruby></li><li><select></select></li></ul></div></"
      "ruby>",
      GetSelectionTextFromBody());
}

TEST_F(InsertListCommandTest, ListifyInputInTableCell1) {
  GetDocument().setDesignMode("on");
  InsertStyleElement(
      "rb { display: table-cell; }"
      "input { float: left; }");
  Selection().SetSelection(
      SetSelectionTextToBody("<div contenteditable='true'><ol><li>^<br></li>"
                             "<li><ruby><rb><input></input></rb></ruby></li>"
                             "<li>XXX</li></ol><div>|</div>"),
      SetSelectionOptions());
  auto* command = MakeGarbageCollected<InsertListCommand>(
      GetDocument(), InsertListCommand::kOrderedList);

  // Crash happens here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<div contenteditable=\"true\">^<br><ol><li><ruby><rb><ol><li><br></li>"
      "<li><ruby><rb><input></rb></ruby></li><li><br></li><li><br></li></ol>"
      "</rb></ruby></li></ol>|XXX<div></div></div>",
      GetSelectionTextFromBody());
}

// Refer https://crbug.com/1295037
TEST_F(InsertListCommandTest, NonCanonicalVisiblePosition) {
  Document& document = GetDocument();
  document.setDesignMode("on");
  InsertStyleElement("select { width: 100vw; }");
  SetBodyInnerHTML(
      "<textarea></textarea><svg></svg><select></select><div><input></div>");
  const Position& base =
      Position::BeforeNode(*document.QuerySelector(AtomicString("select")));
  const Position& extent =
      Position::AfterNode(*document.QuerySelector(AtomicString("input")));
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(base).Extend(extent).Build(),
      SetSelectionOptions());

  // |base| and |extent| are 'canonical' with regard to VisiblePosition.
  ASSERT_EQ(CreateVisiblePosition(base).DeepEquivalent(), base);
  ASSERT_EQ(CreateVisiblePosition(extent).DeepEquivalent(), extent);

  // But |base| is not canonical with regard to CanonicalPositionOf.
  ASSERT_NE(CanonicalPositionOf(base), base);
  ASSERT_EQ(CanonicalPositionOf(extent), extent);

  auto* command = MakeGarbageCollected<InsertListCommand>(
      document, InsertListCommand::kUnorderedList);

  // Crash happens here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<ul><li><textarea></textarea><svg></svg>^<select></select></li>"
      "<li><input>|</li></ul>",
      GetSelectionTextFromBody());
}

// Refer https://crbug.com/1316041
TEST_F(InsertListCommandTest, TimeAndMeterInRoot) {
  Document& document = GetDocument();
  document.setDesignMode("on");

  Element* root = document.documentElement();
  Element* time = document.CreateRawElement(html_names::kTimeTag);
  Element* meter = document.CreateRawElement(html_names::kMeterTag);
  time->appendChild(meter);
  root->insertBefore(time, root->firstChild());

  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(time, 0))
                               .Extend(Position::LastPositionInNode(*time))
                               .Build(),
                           SetSelectionOptions());

  auto* command = MakeGarbageCollected<InsertListCommand>(
      document, InsertListCommand::kUnorderedList);

  // Crash happens here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ("<ul><li>|<time></time></li></ul><head></head><body></body>",
            SelectionSample::GetSelectionText(
                *root, Selection().GetSelectionInDOMTree()));
}

// Refer https://crbug.com/1312348
TEST_F(InsertListCommandTest, PreservedNewline) {
  Document& document = GetDocument();
  document.setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody("<pre><span></span>\nX^<div></div>|</pre>"),
      SetSelectionOptions());

  auto* command = MakeGarbageCollected<InsertListCommand>(
      document, InsertListCommand::kOrderedList);

  // Crash happens here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ("<pre><span></span>\n<ol><li>|X</li></ol><div></div></pre>",
            GetSelectionTextFromBody());
}

// Refer https://crbug.com/1343673
TEST_F(InsertListCommandTest, EmptyInlineBlock) {
  Document& document = GetDocument();
  document.setDesignMode("on");
  InsertStyleElement("span { display: inline-block; min-height: 1px; }");
  Selection().SetSelection(
      SetSelectionTextToBody("<ul><li><span>|</span></li></ul>"),
      SetSelectionOptions());

  auto* command = MakeGarbageCollected<InsertListCommand>(
      document, InsertListCommand::kUnorderedList);

  // Crash happens here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ("<ul><li><span></span></li></ul>|<br>", GetSelectionTextFromBody());
}

// Refer https://crbug.com/1350571
TEST_F(InsertListCommandTest, SelectionFromEndOfTableToAfterTable) {
  Document& document = GetDocument();
  document.setDesignMode("on");
  Selection().SetSelection(SetSelectionTextToBody("<table><td>^</td></table>|"),
                           SetSelectionOptions());

  auto* command = MakeGarbageCollected<InsertListCommand>(
      document, InsertListCommand::kOrderedList);

  // Crash happens here.
  EXPECT_TRUE(command->Apply());
  EXPECT_EQ(
      "<table><tbody><tr><td><ol><li>|<br></li></ol></td></tr></tbody></table>",
      GetSelectionTextFromBody());
}

// Refer https://crbug.com/1366749
TEST_F(InsertListCommandTest, ListItemWithSpace) {
  Document& document = GetDocument();
  document.setDesignMode("on");
  Selection().SetSelection(
      SetSelectionTextToBody(
          "<li>^ <div contenteditable='false'>A</div>B|</li>"),
      SetSelectionOptions());

  auto* command = MakeGarbageCollected<InsertListCommand>(
      document, InsertListCommand::kOrderedList);

  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ("<ul><li> <div contenteditable=\"false\">A</div>B|</li></ul><br>",
            GetSelectionTextFromBody());
}
}
