// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/editing_commands_utilities.h"

#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_div_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class EditingCommandsUtilitiesTest : public EditingTestBase {
 protected:
  void MakeDocumentEmpty();
};

void EditingCommandsUtilitiesTest::MakeDocumentEmpty() {
  while (GetDocument().firstChild())
    GetDocument().RemoveChild(GetDocument().firstChild());
}

TEST_F(EditingCommandsUtilitiesTest, AreaIdenticalElements) {
  SetBodyContent(
      "<style>li:nth-child(even) { -webkit-user-modify: read-write; "
      "}</style><ul><li>first item</li><li>second item</li><li "
      "class=foo>third</li><li>fourth</li></ul>");
  StaticElementList* items =
      GetDocument().QuerySelectorAll(AtomicString("li"), ASSERT_NO_EXCEPTION);
  DCHECK_EQ(items->length(), 4u);

  EXPECT_FALSE(AreIdenticalElements(*items->item(0)->firstChild(),
                                    *items->item(1)->firstChild()))
      << "Can't merge non-elements.  e.g. Text nodes";

  // Compare a LI and a UL.
  EXPECT_FALSE(
      AreIdenticalElements(*items->item(0), *items->item(0)->parentNode()))
      << "Can't merge different tag names.";

  EXPECT_FALSE(AreIdenticalElements(*items->item(0), *items->item(2)))
      << "Can't merge a element with no attributes and another element with an "
         "attribute.";

  // We can't use contenteditable attribute to make editability difference
  // because the hasEquivalentAttributes check is done earier.
  EXPECT_FALSE(AreIdenticalElements(*items->item(0), *items->item(1)))
      << "Can't merge non-editable nodes.";

  EXPECT_TRUE(AreIdenticalElements(*items->item(1), *items->item(3)));
}

TEST_F(EditingCommandsUtilitiesTest, TidyUpHtmlStructureFromBody) {
  auto* body = MakeGarbageCollected<HTMLBodyElement>(GetDocument());
  MakeDocumentEmpty();
  GetDocument().setDesignMode("on");
  GetDocument().AppendChild(body);
  TidyUpHtmlStructure(GetDocument());

  EXPECT_TRUE(IsA<HTMLHtmlElement>(GetDocument().documentElement()));
  EXPECT_EQ(body, GetDocument().body());
  EXPECT_EQ(GetDocument().documentElement(), body->parentNode());
}

TEST_F(EditingCommandsUtilitiesTest, TidyUpHtmlStructureFromDiv) {
  auto* div = MakeGarbageCollected<HTMLDivElement>(GetDocument());
  MakeDocumentEmpty();
  GetDocument().setDesignMode("on");
  GetDocument().AppendChild(div);
  TidyUpHtmlStructure(GetDocument());

  EXPECT_TRUE(IsA<HTMLHtmlElement>(GetDocument().documentElement()));
  EXPECT_TRUE(IsA<HTMLBodyElement>(GetDocument().body()));
  EXPECT_EQ(GetDocument().body(), div->parentNode());
}

TEST_F(EditingCommandsUtilitiesTest, TidyUpHtmlStructureFromHead) {
  auto* head = MakeGarbageCollected<HTMLHeadElement>(GetDocument());
  MakeDocumentEmpty();
  GetDocument().setDesignMode("on");
  GetDocument().AppendChild(head);
  TidyUpHtmlStructure(GetDocument());

  EXPECT_TRUE(IsA<HTMLHtmlElement>(GetDocument().documentElement()));
  EXPECT_TRUE(IsA<HTMLBodyElement>(GetDocument().body()));
  EXPECT_EQ(GetDocument().documentElement(), head->parentNode());
}

TEST_F(EditingCommandsUtilitiesTest, StartOfBlockWithPosition) {
  SetBodyContent("<div id='block'>hello</div>");
  Element* block = GetElementById("block");
  Node* text = block->firstChild();

  // Null position returns null.
  EXPECT_TRUE(StartOfBlock(Position()).IsNull());

  // Position inside text returns first position in enclosing block.
  Position mid(text, 3);
  Position start = StartOfBlock(mid);
  EXPECT_EQ(start, Position::FirstPositionInNode(*block));
}

TEST_F(EditingCommandsUtilitiesTest, EndOfBlockWithPosition) {
  SetBodyContent("<div id='block'>hello</div>");
  Element* block = GetElementById("block");
  Node* text = block->firstChild();

  // Null position returns null.
  EXPECT_TRUE(EndOfBlock(Position()).IsNull());

  // Position inside text returns last position in enclosing block.
  Position mid(text, 3);
  Position end = EndOfBlock(mid);
  EXPECT_EQ(end, Position::LastPositionInNode(*block));
}

TEST_F(EditingCommandsUtilitiesTest, IsStartOfBlockWithPosition) {
  SetBodyContent("<div id='block'>hello</div>");
  Element* block = GetElementById("block");
  Node* text = block->firstChild();

  // Null position returns false.
  EXPECT_FALSE(IsStartOfBlock(Position()));

  // First position in block is start of block.
  EXPECT_TRUE(IsStartOfBlock(Position::FirstPositionInNode(*block)));

  // Mid position is not start of block.
  EXPECT_FALSE(IsStartOfBlock(Position(text, 3)));
}

TEST_F(EditingCommandsUtilitiesTest, IsEndOfBlockWithPosition) {
  SetBodyContent("<div id='block'>hello</div>");
  Element* block = GetElementById("block");
  Node* text = block->firstChild();

  // Null position returns false.
  EXPECT_FALSE(IsEndOfBlock(Position()));

  // Last position in block is end of block.
  EXPECT_TRUE(IsEndOfBlock(Position::LastPositionInNode(*block)));

  // Mid position is not end of block.
  EXPECT_FALSE(IsEndOfBlock(Position(text, 3)));
}

// SnapIntoTableCell -----------------------------------------------------------

TEST_F(EditingCommandsUtilitiesTest, SnapIntoTableCellNullPosition) {
  // A null position (null anchor) is returned unchanged.
  EXPECT_TRUE(SnapIntoTableCell(Position(), TableCellEdge::kStart).IsNull());
}

TEST_F(EditingCommandsUtilitiesTest, SnapIntoTableCellNonTableAnchor) {
  SetBodyContent("<div id='d'>foo</div>");
  Element* div = GetElementById("d");
  // A position anchored on a non-table element is returned unchanged.
  const Position p(div, 0);
  EXPECT_EQ(p, SnapIntoTableCell(p, TableCellEdge::kStart));
}

TEST_F(EditingCommandsUtilitiesTest, SnapIntoTableCellTableAnchorIsExcluded) {
  SetBodyContent("<table id='t'><tbody><tr><td>foo</td></tr></tbody></table>");
  Element* table = GetElementById("t");
  // A position anchored on the <table> element itself is returned unchanged.
  const Position p(table, 0);
  EXPECT_EQ(p, SnapIntoTableCell(p, TableCellEdge::kStart));
}

TEST_F(EditingCommandsUtilitiesTest, SnapIntoTableCellRowAnchorOnCellStart) {
  SetBodyContent(
      "<table><tbody><tr id='r'><td id='c0'>foo</td>"
      "<td id='c1'>bar</td><td id='c2'>baz</td></tr></tbody></table>");
  Element* row = GetElementById("r");
  Element* c1 = GetElementById("c1");
  // (tr, 1) points at the second cell; a start snap descends into it.
  EXPECT_EQ(Position::FirstPositionInNode(*c1),
            SnapIntoTableCell(Position(row, 1), TableCellEdge::kStart));
}

TEST_F(EditingCommandsUtilitiesTest, SnapIntoTableCellRowAnchorOnCellEnd) {
  SetBodyContent(
      "<table><tbody><tr id='r'><td id='c0'>foo</td>"
      "<td id='c1'>bar</td><td id='c2'>baz</td></tr></tbody></table>");
  Element* row = GetElementById("r");
  Element* c1 = GetElementById("c1");
  // (tr, 2) points back at the second cell; an end snap descends into it.
  EXPECT_EQ(Position::LastPositionInNode(*c1),
            SnapIntoTableCell(Position(row, 2), TableCellEdge::kEnd));
}

TEST_F(EditingCommandsUtilitiesTest, SnapIntoTableCellSectionAnchorStart) {
  SetBodyContent(
      "<table><tbody id='b'><tr><td id='c0'>foo</td>"
      "<td id='c2'>baz</td></tr></tbody></table>");
  Element* section = GetElementById("b");
  Element* c0 = GetElementById("c0");
  // (tbody, 0) points at the <tr>; a start snap searches forward to the first
  // cell.
  EXPECT_EQ(Position::FirstPositionInNode(*c0),
            SnapIntoTableCell(Position(section, 0), TableCellEdge::kStart));
}

TEST_F(EditingCommandsUtilitiesTest, SnapIntoTableCellSectionAnchorEnd) {
  SetBodyContent(
      "<table><tbody id='b'><tr><td id='c0'>foo</td>"
      "<td id='c2'>baz</td></tr></tbody></table>");
  Element* section = GetElementById("b");
  Element* c2 = GetElementById("c2");
  // (tbody, 1) points back at the <tr>; an end snap searches back to the last
  // cell.
  EXPECT_EQ(Position::LastPositionInNode(*c2),
            SnapIntoTableCell(Position(section, 1), TableCellEdge::kEnd));
}

TEST_F(EditingCommandsUtilitiesTest, SnapIntoTableCellNoChildAtOffset) {
  SetBodyContent(
      "<table><tbody><tr id='r'><td id='c0'>foo</td></tr></tbody></table>");
  Element* row = GetElementById("r");
  // A start snap past the last child has no node after it: returned unchanged.
  const Position past_end(row, 1);
  EXPECT_EQ(past_end, SnapIntoTableCell(past_end, TableCellEdge::kStart));
  // An end snap before the first child has no node before it: unchanged.
  const Position before_start(row, 0);
  EXPECT_EQ(before_start, SnapIntoTableCell(before_start, TableCellEdge::kEnd));
}

TEST_F(EditingCommandsUtilitiesTest, SnapIntoTableCellNoCellInScope) {
  SetBodyContent("<table><tbody id='b'><tr></tr></tbody></table>");
  Element* section = GetElementById("b");
  // The <tr> contains no cell, so the search finds none and returns unchanged.
  const Position p(section, 0);
  EXPECT_EQ(p, SnapIntoTableCell(p, TableCellEdge::kStart));
}

// SelectionForParagraphIteration (SelectionInDomTree) -------------------------
// Note: the table-boundary adjustments and per-cell snapping inside this
// function are exercised end-to-end by external/wpt/editing/run/indent.html and
// formatblock.html (via ApplyBlockElementCommand under
// EditingUseDomPositionApi) and by the SnapIntoTableCell unit tests above. They
// depend on the caller's surrounding canonicalization and so are not asserted
// in isolation here.

TEST_F(EditingCommandsUtilitiesTest, SelectionForParagraphIterationNone) {
  EXPECT_TRUE(SelectionForParagraphIteration(SelectionInDomTree()).IsNone());
}

TEST_F(EditingCommandsUtilitiesTest, SelectionForParagraphIterationNoTable) {
  const SelectionInDomTree selection =
      SetSelectionTextToBody("<div contenteditable>^foo|</div>");
  EXPECT_EQ(
      "<div contenteditable>^foo|</div>",
      GetSelectionTextFromBody(SelectionForParagraphIteration(selection)));
}

}  // namespace blink
