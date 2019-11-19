// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"

namespace blink {

using ::testing::ElementsAre;

class NGInlineCursorTest : public NGLayoutTest,
                           private ScopedLayoutNGFragmentItemForTest,
                           public testing::WithParamInterface<bool> {
 public:
  NGInlineCursorTest() : ScopedLayoutNGFragmentItemForTest(GetParam()) {}

 protected:
  NGInlineCursor SetupCursor(const String& html) {
    SetBodyInnerHTML(html);
    const LayoutBlockFlow& block_flow =
        *To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
    return NGInlineCursor(block_flow);
  }

  Vector<String> ToDebugStringList(const NGInlineCursor& start) {
    Vector<String> list;
    for (NGInlineCursor cursor(start); cursor; cursor.MoveToNext())
      list.push_back(ToDebugString(cursor));
    return list;
  }

  String ToDebugString(const NGInlineCursor& cursor) {
    if (cursor.IsLineBox())
      return "#linebox";

    if (cursor.IsGeneratedTextType()) {
      StringBuilder result;
      result.Append("#'");
      result.Append(cursor.CurrentText());
      result.Append("'");
      return result.ToString();
    }

    if (cursor.IsText())
      return cursor.CurrentText().ToString().StripWhiteSpace();

    if (const LayoutObject* layout_object = cursor.CurrentLayoutObject()) {
      if (const Element* element =
              DynamicTo<Element>(layout_object->GetNode())) {
        if (const AtomicString& id = element->GetIdAttribute())
          return "#" + id;
      }

      return layout_object->DebugName();
    }

    return "#null";
  }

  Vector<String> ToDebugStringListWithBidiLevel(const NGInlineCursor& start) {
    Vector<String> list;
    for (NGInlineCursor cursor(start); cursor; cursor.MoveToNext())
      list.push_back(ToDebugStringWithBidiLevel(cursor));
    return list;
  }

  String ToDebugStringWithBidiLevel(const NGInlineCursor& cursor) {
    if (!cursor.IsText() && !cursor.IsAtomicInline())
      return ToDebugString(cursor);
    StringBuilder result;
    result.Append(ToDebugString(cursor));
    result.Append(':');
    result.AppendNumber(cursor.CurrentBidiLevel());
    return result.ToString();
  }
};

INSTANTIATE_TEST_SUITE_P(NGInlineCursorTest,
                         NGInlineCursorTest,
                         testing::Bool());

TEST_P(NGInlineCursorTest, BidiLevelInlineBoxLTR) {
  InsertStyleElement("b { display: inline-block; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root dir=ltr>"
      "abc<b id=def>def</b><bdo dir=rtl><b id=ghi>GHI</b></bdo>jkl</div>");
  Vector<String> list = ToDebugStringListWithBidiLevel(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "abc:0", "#def:0",
                                "LayoutInline BDO", "#ghi:1", "jkl:0"));
}

TEST_P(NGInlineCursorTest, BidiLevelInlineBoxRTL) {
  InsertStyleElement("b { display: inline-block; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root dir=rtl>"
      "abc<b id=def>def</b><bdo dir=rtl><b id=ghi>GHI</b></bdo>jkl</div>");
  Vector<String> list = ToDebugStringListWithBidiLevel(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "LayoutInline BDO", "#ghi:3",
                                "jkl:2", "#def:1", "abc:2"));
}

TEST_P(NGInlineCursorTest, BidiLevelSimpleLTR) {
  NGInlineCursor cursor = SetupCursor(
      "<div id=root dir=ltr>"
      "<bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo><br>"
      "123, jkl <bdo dir=rtl>MNO</bdo></div>");
  Vector<String> list = ToDebugStringListWithBidiLevel(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "DEF:1", "abc:2", "GHI:1", ":0",
                                "#linebox", "123, jkl:0", "MNO:1"));
}

TEST_P(NGInlineCursorTest, BidiLevelSimpleRTL) {
  NGInlineCursor cursor = SetupCursor(
      "<div id=root dir=rtl>"
      "<bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo><br>"
      "123, jkl <bdo dir=rtl>MNO</bdo></div>");
  Vector<String> list = ToDebugStringListWithBidiLevel(cursor);
  EXPECT_THAT(
      list, ElementsAre("#linebox", ":0", "DEF:3", "abc:4", "GHI:3", "#linebox",
                        "MNO:3", ":1", "jkl:2", ",:1", "123:2"));
}

TEST_P(NGInlineCursorTest, GetLayoutBlockFlowWithScopedCursor) {
  NGInlineCursor line = SetupCursor("<div id=root>line1<br>line2</div>");
  ASSERT_TRUE(line.IsLineBox()) << line;
  NGInlineCursor cursor = line.CursorForDescendants();
  EXPECT_EQ(line.GetLayoutBlockFlow(), cursor.GetLayoutBlockFlow());
}

TEST_P(NGInlineCursorTest, ContainingLine) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a id=target>def</a>ghi<br>xyz</div>");
  const LayoutBlockFlow& block_flow = *cursor.GetLayoutBlockFlow();
  NGInlineCursor line1(cursor);
  ASSERT_TRUE(line1.IsLineBox());

  NGInlineCursor line2(line1);
  line2.MoveToNextSibling();
  ASSERT_TRUE(line2.IsLineBox());

  cursor.MoveTo(*block_flow.FirstChild());
  cursor.MoveToContainingLine();
  EXPECT_EQ(line1, cursor);

  const LayoutInline& target =
      ToLayoutInline(*GetLayoutObjectByElementId("target"));
  cursor.MoveTo(target);
  cursor.MoveToContainingLine();
  EXPECT_EQ(line1, cursor);

  cursor.MoveTo(*target.FirstChild());
  cursor.MoveToContainingLine();
  EXPECT_EQ(line1, cursor);

  cursor.MoveTo(*block_flow.LastChild());
  cursor.MoveToContainingLine();
  EXPECT_EQ(line2, cursor);
}

TEST_P(NGInlineCursorTest, CulledInlineWithRoot) {
  NGInlineCursor cursor =
      SetupCursor("<div id=root><a><b>abc</b><br><i>xyz</i></a></div>");
  const LayoutInline& layout_inline =
      ToLayoutInline(*cursor.GetLayoutBlockFlow()->FirstChild());
  cursor.MoveTo(layout_inline);
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextForSameLayoutObject();
  }
  EXPECT_THAT(list, ElementsAre("abc", "", "xyz"));
}

TEST_P(NGInlineCursorTest, CulledInlineWithoutRoot) {
  SetBodyInnerHTML(R"HTML(
    <div id="root"><a id="a"><b>abc</b><br><i>xyz</i></a></div>
  )HTML");
  const LayoutObject* layout_inline_a = GetLayoutObjectByElementId("a");
  NGInlineCursor cursor;
  cursor.MoveTo(*layout_inline_a);
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextForSameLayoutObject();
  }
  EXPECT_THAT(list, ElementsAre("abc", "", "xyz"));
}

TEST_P(NGInlineCursorTest, FirstChild) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToFirstChild();
  EXPECT_EQ("abc", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryToMoveToFirstChild());
}

TEST_P(NGInlineCursorTest, FirstChild2) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root><b id=first>abc</b><a>DEF<b>GHI</b></a><a "
      "id=last>xyz</a></div>");
  cursor.MoveToFirstChild();
  EXPECT_EQ("#first", ToDebugString(cursor));
  cursor.MoveToFirstChild();
  EXPECT_EQ("abc", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryToMoveToFirstChild());
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafInSimpleText) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root><b>first</b><b>middle</b><b>last</b></div>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("first", ToDebugString(first_logical_leaf));

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("last", ToDebugString(last_logical_leaf));
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafInRtlText) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
      "<bdo id=root dir=rtl style=display:block>"
      "<b>first</b><b>middle</b><b>last</b>"
      "</bdo>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("first", ToDebugString(first_logical_leaf));

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("last", ToDebugString(last_logical_leaf));
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafInTextAsDeepDescendants) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b><b>first</b>ABC</b>"
      "<b>middle</b>"
      "<b>DEF<b>last</b></b>"
      "</div>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("first", ToDebugString(first_logical_leaf));

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("last", ToDebugString(last_logical_leaf));
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafWithInlineBlock) {
  InsertStyleElement("b { display: inline-block; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b id=first>first</b>middle<b id=last>last</b>"
      "</div>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("#first", ToDebugString(first_logical_leaf))
      << "stop at inline-block";

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("#last", ToDebugString(last_logical_leaf))
      << "stop at inline-block";
}

TEST_P(NGInlineCursorTest, FirstLastLogicalLeafWithImages) {
  NGInlineCursor cursor =
      SetupCursor("<div id=root><img id=first>middle<img id=last></div>");

  NGInlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("#first", ToDebugString(first_logical_leaf));

  NGInlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("#last", ToDebugString(last_logical_leaf));
}

TEST_P(NGInlineCursorTest, LastChild) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToLastChild();
  EXPECT_EQ("xyz", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryToMoveToLastChild());
}

TEST_P(NGInlineCursorTest, LastChild2) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root><b id=first>abc</b><a>DEF<b>GHI</b></a>"
      "<a id=last>xyz</a></div>");
  cursor.MoveToLastChild();
  EXPECT_EQ("#last", ToDebugString(cursor));
  cursor.MoveToLastChild();
  EXPECT_EQ("xyz", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryToMoveToLastChild());
}

TEST_P(NGInlineCursorTest, Next) {
  SetBodyInnerHTML(R"HTML(
    <style>
    span { background: gray; }
    </style>
    <div id=root>
      text1
      <span id="span1">
        text2
        <span id="span2">
          text3
        </span>
        text4
      </span>
      text5
    </div>
  )HTML");

  LayoutBlockFlow* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
  NGInlineCursor cursor(*block_flow);
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "text1", "#span1", "text2",
                                "#span2", "text3", "text4", "text5"));
}

TEST_P(NGInlineCursorTest, NextWithEllipsis) {
  LoadAhem();
  InsertStyleElement(
      "#root {"
      "font: 10px/10px Ahem;"
      "width: 5ch;"
      "overflow-x: hidden;"
      "text-overflow: ellipsis;"
      "}");
  NGInlineCursor cursor = SetupCursor("<div id=root>abcdefghi</div>");
  Vector<String> list = ToDebugStringList(cursor);
  // Note: "abcdefghi" is hidden for paint.
  EXPECT_THAT(list, ElementsAre("#linebox", "abcdefghi", "abcd", u"#'\u2026'"));
}

TEST_P(NGInlineCursorTest, NextWithSoftHyphens) {
  // Use "Ahem" font to get U+2010 as soft hyphen instead of U+002D
  LoadAhem();
  InsertStyleElement("#root {width: 3ch; font: 10px/10px Ahem;}");
  NGInlineCursor cursor = SetupCursor("<div id=root>abc&shy;def</div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", u"abc\u00AD", u"#'\u2010'",
                                "#linebox", "def"));
}

TEST_P(NGInlineCursorTest, NextInlineLeaf) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeaf();
  }
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "DEF", "", "xyz"));
}

TEST_P(NGInlineCursorTest, NextInlineLeafWithEllipsis) {
  LoadAhem();
  InsertStyleElement(
      "#root {"
      "font: 10px/10px Ahem;"
      "width: 5ch;"
      "overflow-x: hidden;"
      "text-overflow: ellipsis;"
      "}");
  NGInlineCursor cursor = SetupCursor("<div id=root>abcdefghi</div>");
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeaf();
  }
  // Note: We don't see hidden for paint and generated soft hyphen.
  // See also |NextWithEllipsis|.
  EXPECT_THAT(list, ElementsAre("#linebox", "abcd"));
}

TEST_P(NGInlineCursorTest, NextInlineLeafWithSoftHyphens) {
  NGInlineCursor cursor =
      SetupCursor("<div id=root style='width:3ch'>abc&shy;def</div>");
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeaf();
  }
  // Note: We don't see generated soft hyphen. See also |NextWithSoftHyphens|.
  EXPECT_THAT(list, ElementsAre("#linebox", u"abc\u00AD", "def"));
}

TEST_P(NGInlineCursorTest, NextInlineLeafIgnoringLineBreak) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeafIgnoringLineBreak();
  }
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "DEF", "xyz"));
}

TEST_P(NGInlineCursorTest, NextLine) {
  NGInlineCursor cursor = SetupCursor("<div id=root>abc<br>xyz</div>");
  NGInlineCursor line1(cursor);
  while (line1 && !line1.IsLineBox())
    line1.MoveToNext();
  ASSERT_TRUE(line1.IsNotNull());
  NGInlineCursor line2(line1);
  line2.MoveToNext();
  while (line2 && !line2.IsLineBox())
    line2.MoveToNext();
  ASSERT_NE(line1, line2);

  NGInlineCursor should_be_line2(line1);
  should_be_line2.MoveToNextLine();
  EXPECT_EQ(line2, should_be_line2);

  NGInlineCursor should_be_null(line2);
  should_be_null.MoveToNextLine();
  EXPECT_TRUE(should_be_null.IsNull());
}

TEST_P(NGInlineCursorTest, NextWithImage) {
  NGInlineCursor cursor = SetupCursor("<div id=root>abc<img id=img>xyz</div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "#img", "xyz"));
}

TEST_P(NGInlineCursorTest, NextWithInlineBox) {
  InsertStyleElement("b { display: inline-block; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<b id=ib>def</b>xyz</div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "#ib", "xyz"));
}

TEST_P(NGInlineCursorTest, NextForSameLayoutObject) {
  NGInlineCursor cursor = SetupCursor("<pre id=root>abc\ndef\nghi</pre>");
  cursor.MoveTo(*GetLayoutObjectByElementId("root")->SlowFirstChild());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextForSameLayoutObject();
  }
  EXPECT_THAT(list, ElementsAre("abc", "", "def", "", "ghi"));
}

TEST_P(NGInlineCursorTest, NextSibling) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToFirstChild();  // go to "abc"
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextSibling();
  }
  EXPECT_THAT(list, ElementsAre("abc", "LayoutInline A", "xyz"));
}

TEST_P(NGInlineCursorTest, NextSibling2) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root><a>abc<b>def</b>xyz</a></div>");
  cursor.MoveToFirstChild();  // go to <a>abc</a>
  cursor.MoveToFirstChild();  // go to "abc"
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextSibling();
  }
  EXPECT_THAT(list, ElementsAre("abc", "LayoutInline B", "xyz"));
}

TEST_P(NGInlineCursorTest, NextSkippingChildren) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("span { background: gray; }");
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      text1
      <span id="span1">
        text2
        <span id="span2">
          text3
        </span>
        text4
      </span>
      text5
    </div>
  )HTML");

  LayoutBlockFlow* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
  NGInlineCursor cursor(*block_flow);
  for (unsigned i = 0; i < 3; ++i)
    cursor.MoveToNext();
  EXPECT_EQ("text2", ToDebugString(cursor));
  Vector<String> list;
  while (true) {
    cursor.MoveToNextSkippingChildren();
    if (!cursor)
      break;
    list.push_back(ToDebugString(cursor));
  }
  EXPECT_THAT(list, ElementsAre("#span2", "text4", "text5"));
}

TEST_P(NGInlineCursorTest, EmptyOutOfFlow) {
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      <span style="position: absolute"></span>
    </div>
  )HTML");

  LayoutBlockFlow* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
  NGInlineCursor cursor(*block_flow);
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre());
}

TEST_P(NGInlineCursorTest, Previous) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  cursor.MoveTo(*cursor.GetLayoutBlockFlow()->LastChild());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPrevious();
  }
  EXPECT_THAT(list, ElementsAre("xyz", "#linebox", "", "DEF", "LayoutInline B",
                                "abc", "#linebox"));
}

TEST_P(NGInlineCursorTest, PreviousInlineLeaf) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  cursor.MoveTo(*cursor.GetLayoutBlockFlow()->LastChild());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPreviousInlineLeaf();
  }
  EXPECT_THAT(list, ElementsAre("xyz", "", "DEF", "abc"));
}

TEST_P(NGInlineCursorTest, PreviousInlineLeafIgnoringLineBreak) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  cursor.MoveTo(*cursor.GetLayoutBlockFlow()->LastChild());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPreviousInlineLeafIgnoringLineBreak();
  }
  EXPECT_THAT(list, ElementsAre("xyz", "DEF", "abc"));
}

TEST_P(NGInlineCursorTest, PreviousLine) {
  NGInlineCursor cursor = SetupCursor("<div id=root>abc<br>xyz</div>");
  NGInlineCursor line1(cursor);
  while (line1 && !line1.IsLineBox())
    line1.MoveToNext();
  ASSERT_TRUE(line1.IsNotNull());
  NGInlineCursor line2(line1);
  line2.MoveToNext();
  while (line2 && !line2.IsLineBox())
    line2.MoveToNext();
  ASSERT_NE(line1, line2);

  NGInlineCursor should_be_null(line1);
  should_be_null.MoveToPreviousLine();
  EXPECT_TRUE(should_be_null.IsNull());

  NGInlineCursor should_be_line1(line2);
  should_be_line1.MoveToPreviousLine();
  EXPECT_EQ(line1, should_be_line1);
}

TEST_P(NGInlineCursorTest, CursorForDescendants) {
  SetBodyInnerHTML(R"HTML(
    <style>
    span { background: yellow; }
    </style>
    <div id=root>
      text1
      <span id="span1">
        text2
        <span id="span2">
          text3
        </span>
        text4
      </span>
      text5
      <span id="span3">
        text6
      </span>
      text7
    </div>
  )HTML");

  LayoutBlockFlow* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
  NGInlineCursor cursor(*block_flow);
  EXPECT_TRUE(cursor.IsLineBox());
  cursor.MoveToNext();
  EXPECT_TRUE(cursor.IsText());
  EXPECT_THAT(ToDebugStringList(cursor.CursorForDescendants()), ElementsAre());
  cursor.MoveToNext();
  EXPECT_EQ(ToDebugString(cursor), "#span1");
  EXPECT_THAT(ToDebugStringList(cursor.CursorForDescendants()),
              ElementsAre("text2", "#span2", "text3", "text4"));
  cursor.MoveToNext();
  EXPECT_EQ(ToDebugString(cursor), "text2");
  EXPECT_THAT(ToDebugStringList(cursor.CursorForDescendants()), ElementsAre());
  cursor.MoveToNext();
  EXPECT_EQ(ToDebugString(cursor), "#span2");
  EXPECT_THAT(ToDebugStringList(cursor.CursorForDescendants()),
              ElementsAre("text3"));
}

}  // namespace blink
