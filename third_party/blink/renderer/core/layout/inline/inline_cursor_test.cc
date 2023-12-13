// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/inline/inline_node_data.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

namespace {

using ::testing::ElementsAre;

String ToDebugString(const InlineCursor& cursor) {
  if (cursor.Current().IsLineBox())
    return "#linebox";

  if (cursor.Current().IsLayoutGeneratedText()) {
    StringBuilder result;
    result.Append("#'");
    result.Append(cursor.CurrentText());
    result.Append("'");
    return result.ToString();
  }

  if (cursor.Current().IsText())
    return cursor.CurrentText().ToString().StripWhiteSpace();

  if (const LayoutObject* layout_object = cursor.Current().GetLayoutObject()) {
    if (const Element* element = DynamicTo<Element>(layout_object->GetNode())) {
      if (const AtomicString& id = element->GetIdAttribute())
        return "#" + id;
    }

    return layout_object->DebugName();
  }

  return "#null";
}

Vector<String> LayoutObjectToDebugStringList(InlineCursor cursor) {
  Vector<String> list;
  for (; cursor; cursor.MoveToNextForSameLayoutObject())
    list.push_back(ToDebugString(cursor));
  return list;
}

class InlineCursorTest : public RenderingTest,
                         public testing::WithParamInterface<bool> {
 protected:
  InlineCursor SetupCursor(const String& html) {
    SetBodyInnerHTML(html);
    const LayoutBlockFlow& block_flow =
        *To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
    return InlineCursor(block_flow);
  }

  Vector<String> ToDebugStringList(const InlineCursor& start) {
    Vector<String> list;
    for (InlineCursor cursor(start); cursor; cursor.MoveToNext()) {
      list.push_back(ToDebugString(cursor));
    }
    return list;
  }

  Vector<String> SiblingsToDebugStringList(const InlineCursor& start) {
    Vector<String> list;
    for (InlineCursor cursor(start); cursor;
         cursor.MoveToNextSkippingChildren()) {
      list.push_back(ToDebugString(cursor));
    }
    return list;
  }

  // Test |MoveToNextSibling| and |InlineBackwardCursor| return the same
  // instances, except that the order is reversed.
  void TestPrevoiusSibling(const InlineCursor& start) {
    DCHECK(start.HasRoot());
    Vector<const FragmentItem*> forwards;
    for (InlineCursor cursor(start); cursor;
         cursor.MoveToNextSkippingChildren()) {
      forwards.push_back(cursor.CurrentItem());
    }
    Vector<const FragmentItem*> backwards;
    for (InlineBackwardCursor cursor(start); cursor;
         cursor.MoveToPreviousSibling()) {
      backwards.push_back(cursor.Current().Item());
    }
    backwards.Reverse();
    EXPECT_THAT(backwards, forwards);
  }

  Vector<String> ToDebugStringListWithBidiLevel(const InlineCursor& start) {
    Vector<String> list;
    for (InlineCursor cursor(start); cursor; cursor.MoveToNext()) {
      // Inline boxes do not have bidi level.
      if (cursor.Current().IsInlineBox())
        continue;
      list.push_back(ToDebugStringWithBidiLevel(cursor));
    }
    return list;
  }

  String ToDebugStringWithBidiLevel(const InlineCursor& cursor) {
    if (!cursor.Current().IsText() && !cursor.Current().IsAtomicInline())
      return ToDebugString(cursor);
    StringBuilder result;
    result.Append(ToDebugString(cursor));
    result.Append(':');
    result.AppendNumber(cursor.Current().BidiLevel());
    return result.ToString();
  }
};

INSTANTIATE_TEST_SUITE_P(InlineCursorTest, InlineCursorTest, testing::Bool());

TEST_P(InlineCursorTest, BidiLevelInlineBoxLTR) {
  InsertStyleElement("b { display: inline-block; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root dir=ltr>"
      "abc<b id=def>def</b><bdo dir=rtl><b id=ghi>GHI</b></bdo>jkl</div>");
  Vector<String> list = ToDebugStringListWithBidiLevel(cursor);
  EXPECT_THAT(list,
              ElementsAre("#linebox", "abc:0", "#def:0", "#ghi:1", "jkl:0"));
}

TEST_P(InlineCursorTest, BidiLevelInlineBoxRTL) {
  InsertStyleElement("b { display: inline-block; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root dir=rtl>"
      "abc<b id=def>def</b><bdo dir=rtl><b id=ghi>GHI</b></bdo>jkl</div>");
  Vector<String> list = ToDebugStringListWithBidiLevel(cursor);
  EXPECT_THAT(list,
              ElementsAre("#linebox", "#ghi:3", "jkl:2", "#def:1", "abc:2"));
}

TEST_P(InlineCursorTest, BidiLevelSimpleLTR) {
  InlineCursor cursor = SetupCursor(
      "<div id=root dir=ltr>"
      "<bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo><br>"
      "123, jkl <bdo dir=rtl>MNO</bdo></div>");
  Vector<String> list = ToDebugStringListWithBidiLevel(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "DEF:1", "abc:2", "GHI:1", ":0",
                                "#linebox", "123, jkl:0", "MNO:1"));
}

TEST_P(InlineCursorTest, BidiLevelSimpleRTL) {
  InlineCursor cursor = SetupCursor(
      "<div id=root dir=rtl>"
      "<bdo dir=rtl>GHI<bdo dir=ltr>abc</bdo>DEF</bdo><br>"
      "123, jkl <bdo dir=rtl>MNO</bdo></div>");
  Vector<String> list = ToDebugStringListWithBidiLevel(cursor);
  EXPECT_THAT(
      list, ElementsAre("#linebox", ":0", "DEF:3", "abc:4", "GHI:3", "#linebox",
                        "MNO:3", ":1", "jkl:2", ",:1", "123:2"));
}

TEST_P(InlineCursorTest, GetLayoutBlockFlowWithScopedCursor) {
  InlineCursor line = SetupCursor("<div id=root>line1<br>line2</div>");
  ASSERT_TRUE(line.Current().IsLineBox()) << line;
  InlineCursor cursor = line.CursorForDescendants();
  EXPECT_EQ(line.GetLayoutBlockFlow(), cursor.GetLayoutBlockFlow());
}

TEST_P(InlineCursorTest, Parent) {
  InlineCursor cursor = SetupCursor(R"HTML(
    <style>
    span { background: yellow; } /* Ensure not culled. */
    </style>
    <body>
      <div id="root">
        text1
        <span id="span1">
          span1
          <span></span>
          <span id="span2">
            span2
            <span style="display: inline-block"></span>
            <span id="span3">
              span3
            </span>
          </span>
        </span>
      </div>
    <body>
)HTML");
  cursor.MoveTo(*GetLayoutObjectByElementId("span3"));
  ASSERT_TRUE(cursor);
  Vector<AtomicString> ids;
  for (;;) {
    cursor.MoveToParent();
    if (!cursor)
      break;
    const auto* element = To<Element>(cursor.Current()->GetNode());
    ids.push_back(element->GetIdAttribute());
  }
  EXPECT_THAT(ids, testing::ElementsAre("span2", "span1", "root"));
}

TEST_P(InlineCursorTest, ContainingLine) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  InlineCursor cursor =
      SetupCursor("<div id=root>abc<a id=target>def</a>ghi<br>xyz</div>");
  const LayoutBlockFlow& block_flow = *cursor.GetLayoutBlockFlow();
  InlineCursor line1(cursor);
  ASSERT_TRUE(line1.Current().IsLineBox());

  InlineCursor line2(line1);
  line2.MoveToNextSkippingChildren();
  ASSERT_TRUE(line2.Current().IsLineBox());

  cursor.MoveTo(*block_flow.FirstChild());
  cursor.MoveToContainingLine();
  EXPECT_EQ(line1, cursor);

  const auto& target = To<LayoutInline>(*GetLayoutObjectByElementId("target"));
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

TEST_P(InlineCursorTest, CulledInlineWithAtomicInline) {
  SetBodyInnerHTML(
      "<div id=root>"
      "<b id=culled>abc<div style=display:inline>ABC<br>XYZ</div>xyz</b>"
      "</div>");
  InlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
              ElementsAre("abc", "ABC", "", "XYZ", "xyz"));
}

// We should not have float:right fragment, because it isn't in-flow in
// an inline formatting context.
// For https://crbug.com/1026022
TEST_P(InlineCursorTest, CulledInlineWithFloat) {
  SetBodyInnerHTML(
      "<div id=root>"
      "<b id=culled>abc<div style=float:right></div>xyz</b>"
      "</div>");
  InlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("abc", "xyz"));
}

TEST_P(InlineCursorTest, CulledInlineWithOOF) {
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      <b id=culled>abc<span style="position:absolute"></span>xyz</b>
    </div>
  )HTML");
  InlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("abc", "xyz"));
}

TEST_P(InlineCursorTest, CulledInlineNested) {
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      <b id=culled><span>abc</span> xyz</b>
    </div>
  )HTML");
  InlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("abc", "xyz"));
}

TEST_P(InlineCursorTest, CulledInlineBlockChild) {
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      <b id=culled>
        <div>block</div>
        <span>abc</span> xyz
      </b>
    </div>
  )HTML");
  InlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
              ElementsAre("#culled", "#culled", "#culled"));
}

TEST_P(InlineCursorTest, CulledInlineWithRoot) {
  InlineCursor cursor = SetupCursor(R"HTML(
    <div id="root"><a id="a"><b>abc</b><br><i>xyz</i></a></div>
  )HTML");
  const LayoutObject* layout_inline_a = GetLayoutObjectByElementId("a");
  cursor.MoveToIncludingCulledInline(*layout_inline_a);
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
              ElementsAre("abc", "", "xyz"));
}

TEST_P(InlineCursorTest, CulledInlineWithoutRoot) {
  SetBodyInnerHTML(R"HTML(
    <div id="root"><a id="a"><b>abc</b><br><i>xyz</i></a></div>
  )HTML");
  const LayoutObject* layout_inline_a = GetLayoutObjectByElementId("a");
  InlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*layout_inline_a);
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
              ElementsAre("abc", "", "xyz"));
}

TEST_P(InlineCursorTest, CursorForMovingAcrossFragmentainer) {
  LoadAhem();
  InsertStyleElement(
      "div { font: 10px/15px Ahem; column-count: 2; width: 20ch; }");
  SetBodyInnerHTML("<div id=m>abc<br>def<br><b id=t>ghi</b><br>jkl<br></div>");
  // The HTML is rendered as:
  //    abc ghi
  //    def jkl

  // MoveTo(LayoutObject) makes |InlineCursor| to be able to move across
  // fragmentainer.
  InlineCursor cursor;
  cursor.MoveTo(*GetElementById("t")->firstChild()->GetLayoutObject());
  ASSERT_TRUE(cursor.IsBlockFragmented()) << cursor;

  InlineCursor cursor2(cursor.ContainerFragment());
  ASSERT_FALSE(cursor2.IsBlockFragmented()) << cursor2;
  cursor2.MoveTo(*cursor.CurrentItem());
  ASSERT_FALSE(cursor2.IsBlockFragmented());

  InlineCursor cursor3 = cursor2.CursorForMovingAcrossFragmentainer();
  EXPECT_TRUE(cursor3.IsBlockFragmented()) << cursor3;
  EXPECT_EQ(&cursor2.ContainerFragment(), &cursor3.ContainerFragment());
  EXPECT_EQ(cursor2.CurrentItem(), cursor3.CurrentItem());
}

TEST_P(InlineCursorTest, FirstChild) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  InlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToFirstChild();
  EXPECT_EQ("abc", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryMoveToFirstChild());
}

TEST_P(InlineCursorTest, FirstChild2) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root><b id=first>abc</b><a>DEF<b>GHI</b></a><a "
      "id=last>xyz</a></div>");
  cursor.MoveToFirstChild();
  EXPECT_EQ("#first", ToDebugString(cursor));
  cursor.MoveToFirstChild();
  EXPECT_EQ("abc", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryMoveToFirstChild());
}

TEST_P(InlineCursorTest, FirstLastLogicalLeafInSimpleText) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor =
      SetupCursor("<div id=root><b>first</b><b>middle</b><b>last</b></div>");

  InlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("first", ToDebugString(first_logical_leaf));

  InlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("last", ToDebugString(last_logical_leaf));
}

TEST_P(InlineCursorTest, FirstLastLogicalLeafInRtlText) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor(
      "<bdo id=root dir=rtl style=display:block>"
      "<b>first</b><b>middle</b><b>last</b>"
      "</bdo>");

  InlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("first", ToDebugString(first_logical_leaf));

  InlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("last", ToDebugString(last_logical_leaf));
}

TEST_P(InlineCursorTest, FirstLastLogicalLeafInTextAsDeepDescendants) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b><b>first</b>ABC</b>"
      "<b>middle</b>"
      "<b>DEF<b>last</b></b>"
      "</div>");

  InlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("first", ToDebugString(first_logical_leaf));

  InlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("last", ToDebugString(last_logical_leaf));
}

TEST_P(InlineCursorTest, FirstLastLogicalLeafWithInlineBlock) {
  InsertStyleElement("b { display: inline-block; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b id=first>first</b>middle<b id=last>last</b>"
      "</div>");

  InlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("#first", ToDebugString(first_logical_leaf))
      << "stop at inline-block";

  InlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("#last", ToDebugString(last_logical_leaf))
      << "stop at inline-block";
}

TEST_P(InlineCursorTest, FirstLastLogicalLeafWithImages) {
  InlineCursor cursor =
      SetupCursor("<div id=root><img id=first>middle<img id=last></div>");

  InlineCursor first_logical_leaf(cursor);
  first_logical_leaf.MoveToFirstLogicalLeaf();
  EXPECT_EQ("#first", ToDebugString(first_logical_leaf));

  InlineCursor last_logical_leaf(cursor);
  last_logical_leaf.MoveToLastLogicalLeaf();
  EXPECT_EQ("#last", ToDebugString(last_logical_leaf));
}

// http://crbug.com/1295087
TEST_P(InlineCursorTest, FirstNonPseudoLeafWithBlockImage) {
  InsertStyleElement("img { display: block; }");
  InlineCursor cursor = SetupCursor("<p id=root><b><img id=target></b></p>");

  // Note: The first child of block-in-inline can be |LayoutImage|.
  // LayoutNGBlockFlow P id="root"
  //   +--LayoutInline SPAN
  //   |  +--LayoutNGBlockFlow (anonymous)  # block-in-inline
  //   |  |  +--LayoutImage IMG id="target" # first child of block-in-inline
  //   +--LayoutText #text ""
  const auto& target =
      *To<LayoutImage>(GetElementById("target")->GetLayoutObject());

  cursor.MoveToFirstNonPseudoLeaf();
  ASSERT_TRUE(cursor.Current());
  EXPECT_EQ(target.Parent(), cursor.Current().GetLayoutObject());
  ASSERT_TRUE(cursor.Current()->IsBlockInInline());
  EXPECT_EQ(&target, cursor.Current()->BlockInInline());
}

TEST_P(InlineCursorTest, IsEmptyLineBox) {
  InsertStyleElement("b { margin-bottom: 1px; }");
  InlineCursor cursor = SetupCursor("<div id=root>abc<br><b></b></div>");

  EXPECT_FALSE(cursor.Current().IsEmptyLineBox())
      << "'abc\\n' is in non-empty line box.";
  cursor.MoveToNextLine();
  EXPECT_TRUE(cursor.Current().IsEmptyLineBox())
      << "<b></b> with margin produces empty line box.";
}

TEST_P(InlineCursorTest, LastChild) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  InlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToLastChild();
  EXPECT_EQ("xyz", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryMoveToLastChild());
}

TEST_P(InlineCursorTest, LastChild2) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root><b id=first>abc</b><a>DEF<b>GHI</b></a>"
      "<a id=last>xyz</a></div>");
  cursor.MoveToLastChild();
  EXPECT_EQ("#last", ToDebugString(cursor));
  cursor.MoveToLastChild();
  EXPECT_EQ("xyz", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryMoveToLastChild());
}

TEST_P(InlineCursorTest, Next) {
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
  InlineCursor cursor(*block_flow);
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "text1", "#span1", "text2",
                                "#span2", "text3", "text4", "text5"));
}

TEST_P(InlineCursorTest, NextIncludingFragmentainer) {
  // TDOO(yosin): Remove style for <b> once FragmentItem don't do culled
  // inline.
  LoadAhem();
  InsertStyleElement(
      "b { background: gray; }"
      "div { font: 10px/15px Ahem; column-count: 2; width: 20ch; }");
  SetBodyInnerHTML("<div id=m>abc<br>def<br><b>ghi</b><br>jkl</div>");
  InlineCursor cursor;
  cursor.MoveTo(*GetElementById("m")->firstChild()->GetLayoutObject());
  ASSERT_TRUE(cursor.IsBlockFragmented()) << cursor;
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextIncludingFragmentainer();
  }
  EXPECT_THAT(list,
              ElementsAre("abc", "", "#linebox", "def", "", "#linebox",
                          "LayoutInline B", "ghi", "", "#linebox", "jkl"));
}

TEST_P(InlineCursorTest, NextWithEllipsis) {
  LoadAhem();
  InsertStyleElement(
      "#root {"
      "font: 10px/10px Ahem;"
      "width: 5ch;"
      "overflow-x: hidden;"
      "text-overflow: ellipsis;"
      "}");
  InlineCursor cursor = SetupCursor("<div id=root>abcdefghi</div>");
  Vector<String> list = ToDebugStringList(cursor);
  // Note: "abcdefghi" is hidden for paint.
  EXPECT_THAT(list, ElementsAre("#linebox", "abcdefghi", "abcd", u"#'\u2026'"));
}

TEST_P(InlineCursorTest, NextWithEllipsisInlineBoxOnly) {
  LoadAhem();
  InsertStyleElement(
      "#root {"
      "font: 10px/1 Ahem;"
      "width: 5ch;"
      "overflow: hidden;"
      "text-overflow: ellipsis;"
      "}"
      "span { border: solid 10ch blue; }");
  InlineCursor cursor = SetupCursor("<div id=root><span></span></div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "LayoutInline SPAN"));
}

TEST_P(InlineCursorTest, NextWithListItem) {
  InlineCursor cursor = SetupCursor("<ul><li id=root>abc</li></ul>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(
      list, ElementsAre("LayoutOutsideListMarker ::marker", "#linebox", "abc"));
  EXPECT_EQ(GetLayoutObjectByElementId("root"), cursor.GetLayoutBlockFlow());
}

TEST_P(InlineCursorTest, NextWithSoftHyphens) {
  // Use "Ahem" font to get U+2010 as soft hyphen instead of U+002D
  LoadAhem();
  InsertStyleElement("#root {width: 3ch; font: 10px/10px Ahem;}");
  InlineCursor cursor = SetupCursor("<div id=root>abc&shy;def</div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", u"abc\u00AD", u"#'\u2010'",
                                "#linebox", "def"));
}

TEST_P(InlineCursorTest, NextInlineLeaf) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeaf();
  }
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "DEF", "", "xyz"));
}

// Note: This is for AccessibilityLayoutTest.NextOnLine.
TEST_P(InlineCursorTest, NextInlineLeafOnLineFromLayoutInline) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b id=start>abc</b> def<br>"
      "<b>ABC</b> DEF<br>"
      "</div>");
  cursor.MoveTo(*GetElementById("start")->GetLayoutObject());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeafOnLine();
  }
  EXPECT_THAT(list, ElementsAre("#start", "def", ""))
      << "we don't have 'abc' and items in second line.";
}

TEST_P(InlineCursorTest, NextInlineLeafOnLineFromNestedLayoutInline) {
  // Never return a descendant for AXLayoutObject::NextOnLine().
  // Instead, if NextOnLine() is called on a container, return the first
  // content from a sibling subtree.
  InlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<span id=start>"
      "Test<span style=font-size:13px>descendant</span>"
      "</span>"
      "<span>next</span>"
      "</div>");
  cursor.MoveToIncludingCulledInline(
      *GetElementById("start")->GetLayoutObject());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeafOnLine();
  }
  EXPECT_THAT(list, ElementsAre("#start", "next"))
      << "next on line doesn't return descendant.";
}

TEST_P(InlineCursorTest, NextInlineLeafOnLineFromLayoutText) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b id=start>abc</b> def<br>"
      "<b>ABC</b> DEF<br>"
      "</div>");
  cursor.MoveTo(*GetElementById("start")->firstChild()->GetLayoutObject());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeafOnLine();
  }
  EXPECT_THAT(list, ElementsAre("abc", "def", ""))
      << "We don't have items from second line.";
}

TEST_P(InlineCursorTest, NextInlineLeafWithEllipsis) {
  LoadAhem();
  InsertStyleElement(
      "#root {"
      "font: 10px/10px Ahem;"
      "width: 5ch;"
      "overflow-x: hidden;"
      "text-overflow: ellipsis;"
      "}");
  InlineCursor cursor = SetupCursor("<div id=root>abcdefghi</div>");
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeaf();
  }
  // Note: We don't see hidden for paint and generated soft hyphen.
  // See also |NextWithEllipsis|.
  EXPECT_THAT(list, ElementsAre("#linebox", "abcd"));
}

TEST_P(InlineCursorTest, NextInlineLeafWithSoftHyphens) {
  InlineCursor cursor =
      SetupCursor("<div id=root style='width:3ch'>abc&shy;def</div>");
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeaf();
  }
  // Note: We don't see generated soft hyphen. See also |NextWithSoftHyphens|.
  EXPECT_THAT(list, ElementsAre("#linebox", u"abc\u00AD", "def"));
}

TEST_P(InlineCursorTest, NextInlineLeafIgnoringLineBreak) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToNextInlineLeafIgnoringLineBreak();
  }
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "DEF", "xyz"));
}

TEST_P(InlineCursorTest, NextLine) {
  InlineCursor cursor = SetupCursor("<div id=root>abc<br>xyz</div>");
  InlineCursor line1(cursor);
  while (line1 && !line1.Current().IsLineBox())
    line1.MoveToNext();
  ASSERT_TRUE(line1.IsNotNull());
  InlineCursor line2(line1);
  line2.MoveToNext();
  while (line2 && !line2.Current().IsLineBox())
    line2.MoveToNext();
  ASSERT_NE(line1, line2);

  InlineCursor should_be_line2(line1);
  should_be_line2.MoveToNextLine();
  EXPECT_EQ(line2, should_be_line2);

  InlineCursor should_be_null(line2);
  should_be_null.MoveToNextLine();
  EXPECT_TRUE(should_be_null.IsNull());
}

TEST_P(InlineCursorTest, NextWithImage) {
  InlineCursor cursor = SetupCursor("<div id=root>abc<img id=img>xyz</div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "#img", "xyz"));
}

TEST_P(InlineCursorTest, NextWithInlineBox) {
  InsertStyleElement("b { display: inline-block; }");
  InlineCursor cursor =
      SetupCursor("<div id=root>abc<b id=ib>def</b>xyz</div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "abc", "#ib", "xyz"));

  InlineCursor cursor2;
  cursor2.MoveTo(*GetElementById("ib")->firstChild()->GetLayoutObject());
  EXPECT_EQ(GetLayoutObjectByElementId("ib"), cursor2.GetLayoutBlockFlow());
}

TEST_P(InlineCursorTest, NextForSameLayoutObject) {
  InlineCursor cursor = SetupCursor("<pre id=root>abc\ndef\nghi</pre>");
  cursor.MoveTo(*GetLayoutObjectByElementId("root")->SlowFirstChild());
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
              ElementsAre("abc", "", "def", "", "ghi"));
}

// Test |NextForSameLayoutObject| with limit range set.
TEST_P(InlineCursorTest, NextForSameLayoutObjectWithRange) {
  // In this snippet, `<span>` wraps to 3 lines, and that there are 3 fragments
  // for `<span>`.
  SetBodyInnerHTML(R"HTML(
    <style>
    div {
      font-size: 10px;
      width: 5ch;
    }
    span {
      background: orange;
    }
    </style>
    <div id="root">
      <span id="span1">
        1111
        2222
        3333
      </span>
    </div>
  )HTML");
  LayoutBlockFlow* root =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
  InlineCursor cursor(*root);
  cursor.MoveToFirstLine();
  cursor.MoveToNextLine();
  InlineCursor line2 = cursor.CursorForDescendants();

  // Now |line2| is limited to the 2nd line. There should be only one framgnet
  // for `<span>` if we search using `line2`.
  LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  line2.MoveTo(*span1);
  EXPECT_THAT(LayoutObjectToDebugStringList(line2), ElementsAre("#span1"));
}

TEST_P(InlineCursorTest, Sibling) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  InlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  TestPrevoiusSibling(cursor.CursorForDescendants());
  cursor.MoveToFirstChild();  // go to "abc"
  Vector<String> list = SiblingsToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("abc", "LayoutInline A", "xyz"));
}

TEST_P(InlineCursorTest, Sibling2) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  InlineCursor cursor =
      SetupCursor("<div id=root><a>abc<b>def</b>xyz</a></div>");
  cursor.MoveToFirstChild();  // go to <a>abc</a>
  TestPrevoiusSibling(cursor.CursorForDescendants());
  cursor.MoveToFirstChild();  // go to "abc"
  Vector<String> list = SiblingsToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("abc", "LayoutInline B", "xyz"));
}

TEST_P(InlineCursorTest, NextSkippingChildren) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
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
  InlineCursor cursor(*block_flow);
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

TEST_P(InlineCursorTest, EmptyOutOfFlow) {
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      <span style="position: absolute"></span>
    </div>
  )HTML");

  LayoutBlockFlow* block_flow =
      To<LayoutBlockFlow>(GetLayoutObjectByElementId("root"));
  InlineCursor cursor(*block_flow);
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre());
}

TEST_P(InlineCursorTest, PositionForPointInChildHorizontalLTR) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "direction: ltr;"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: horizontal-tb;"
      "}");
  InlineCursor cursor = SetupCursor("<p id=root>ab</p>");
  const auto& text = *To<Text>(GetElementById("root")->firstChild());
  ASSERT_TRUE(cursor.Current().IsLineBox());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(10, 10), PhysicalSize(20, 20)),
            cursor.Current().RectInContainerFragment());

  cursor.MoveTo(*text.GetLayoutObject());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(10, 15), PhysicalSize(20, 10)),
            cursor.Current().RectInContainerFragment());
  const PhysicalOffset left_top = cursor.Current().OffsetInContainerFragment();

  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(-5, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top));
  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(5, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 1)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(10, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 1)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(15, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(20, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(25, 0)));
}

TEST_P(InlineCursorTest, PositionForPointInChildHorizontalRTL) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "direction: rtl;"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: horizontal-tb;"
      "}");
  InlineCursor cursor = SetupCursor("<p id=root><bdo dir=rtl>AB</bdo></p>");
  const auto& text =
      *To<Text>(GetElementById("root")->firstChild()->firstChild());
  ASSERT_TRUE(cursor.Current().IsLineBox());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(754, 10), PhysicalSize(20, 20)),
            cursor.Current().RectInContainerFragment());

  cursor.MoveTo(*text.GetLayoutObject());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(754, 15), PhysicalSize(20, 10)),
            cursor.Current().RectInContainerFragment());
  const PhysicalOffset left_top = cursor.Current().OffsetInContainerFragment();

  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(-5, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top));
  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(5, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 1)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(10, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 1)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(15, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(20, 0)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(25, 0)));
}

TEST_P(InlineCursorTest, PositionForPointInChildVerticalLTR) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "direction: ltr;"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: vertical-lr;"
      "}");
  InlineCursor cursor = SetupCursor("<p id=root>ab</p>");
  const auto& text = *To<Text>(GetElementById("root")->firstChild());
  ASSERT_TRUE(cursor.Current().IsLineBox());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(10, 10), PhysicalSize(20, 20)),
            cursor.Current().RectInContainerFragment());

  cursor.MoveTo(*text.GetLayoutObject());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(15, 10), PhysicalSize(10, 20)),
            cursor.Current().RectInContainerFragment());
  const PhysicalOffset left_top = cursor.Current().OffsetInContainerFragment();

  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, -5)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top));
  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 5)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 1)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 10)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 1)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 15)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 20)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 25)));
}

TEST_P(InlineCursorTest, PositionForPointInChildVerticalRTL) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "direction: rtl;"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: vertical-rl;"
      "}");
  InlineCursor cursor = SetupCursor("<p id=root><bdo dir=rtl>AB</bdo></p>");
  const auto& text =
      *To<Text>(GetElementById("root")->firstChild()->firstChild());
  ASSERT_TRUE(cursor.Current().IsLineBox());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(10, 10), PhysicalSize(20, 20)),
            cursor.Current().RectInContainerFragment());

  cursor.MoveTo(*text.GetLayoutObject());
  EXPECT_EQ(PhysicalRect(PhysicalOffset(15, 10), PhysicalSize(10, 20)),
            cursor.Current().RectInContainerFragment());
  const PhysicalOffset left_top = cursor.Current().OffsetInContainerFragment();

  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, -5)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top));
  EXPECT_EQ(PositionWithAffinity(Position(text, 2), TextAffinity::kUpstream),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 5)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 1)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 10)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 1)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 15)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 20)));
  EXPECT_EQ(PositionWithAffinity(Position(text, 0)),
            cursor.PositionForPointInChild(left_top + PhysicalOffset(0, 25)));
}

// For http://crbug.com/1096110
TEST_P(InlineCursorTest, PositionForPointInChildBlockChildren) {
  InsertStyleElement("b { display: inline-block; }");
  // Note: <b>.ChildrenInline() == false
  InlineCursor cursor =
      SetupCursor("<div id=root>a<b id=target><div>x</div></b></div>");
  const Element& target = *GetElementById("target");
  cursor.MoveTo(*target.GetLayoutObject());
  EXPECT_EQ(PositionWithAffinity(Position::FirstPositionInNode(target)),
            cursor.PositionForPointInChild(PhysicalOffset()));
}

TEST_P(InlineCursorTest, Previous) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  cursor.MoveTo(*cursor.GetLayoutBlockFlow()->LastChild());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPrevious();
  }
  EXPECT_THAT(list, ElementsAre("xyz", "#linebox", "", "DEF", "LayoutInline B",
                                "abc", "#linebox"));
}

TEST_P(InlineCursorTest, PreviousIncludingFragmentainer) {
  // TDOO(yosin): Remove style for <b> once FragmentItem don't do culled
  // inline.
  LoadAhem();
  InsertStyleElement(
      "b { background: gray; }"
      "div { font: 10px/15px Ahem; column-count: 2; width: 20ch; }");
  SetBodyInnerHTML("<div id=m>abc<br>def<br><b>ghi</b><br>jkl</div>");
  InlineCursor cursor;
  cursor.MoveTo(*GetElementById("m")->lastChild()->GetLayoutObject());
  ASSERT_TRUE(cursor.IsBlockFragmented()) << cursor;
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPreviousIncludingFragmentainer();
  }
  EXPECT_THAT(list, ElementsAre("jkl", "#linebox", "", "ghi", "LayoutInline B",
                                "#linebox", "", "def", "#linebox", "", "abc",
                                "#linebox"));
}

TEST_P(InlineCursorTest, PreviousInlineLeaf) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  cursor.MoveTo(*cursor.GetLayoutBlockFlow()->LastChild());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPreviousInlineLeaf();
  }
  EXPECT_THAT(list, ElementsAre("xyz", "", "DEF", "abc"));
}

TEST_P(InlineCursorTest, PreviousInlineLeafIgnoringLineBreak) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor("<div id=root>abc<b>DEF</b><br>xyz</div>");
  cursor.MoveTo(*cursor.GetLayoutBlockFlow()->LastChild());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPreviousInlineLeafIgnoringLineBreak();
  }
  EXPECT_THAT(list, ElementsAre("xyz", "DEF", "abc"));
}

TEST_P(InlineCursorTest, PreviousInlineLeafOnLineFromLayoutInline) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b>abc</b> def<br>"
      "<b>ABC</b> <b id=start>DEF</b><br>"
      "</div>");
  cursor.MoveTo(*GetElementById("start")->GetLayoutObject());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPreviousInlineLeafOnLine();
  }
  EXPECT_THAT(list, ElementsAre("#start", "", "ABC"))
      << "We don't have 'DEF' and items in first line.";
}

TEST_P(InlineCursorTest, PreviousInlineLeafOnLineFromNestedLayoutInline) {
  // Never return a descendant for AXLayoutObject::PreviousOnLine().
  // Instead, if PreviousOnLine() is called on a container, return a previpus
  // item from the previous siblings subtree.
  InlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<span>previous</span>"
      "<span id=start>"
      "Test<span style=font-size:13px>descendant</span>"
      "</span>"
      "</div>");
  cursor.MoveToIncludingCulledInline(
      *GetElementById("start")->GetLayoutObject());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPreviousInlineLeafOnLine();
  }
  EXPECT_THAT(list, ElementsAre("#start", "previous"))
      << "previous on line doesn't return descendant.";
}

TEST_P(InlineCursorTest, PreviousInlineLeafOnLineFromLayoutText) {
  // TDOO(yosin): Remove <style> once FragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  InlineCursor cursor = SetupCursor(
      "<div id=root>"
      "<b>abc</b> def<br>"
      "<b>ABC</b> <b id=start>DEF</b><br>"
      "</div>");
  cursor.MoveTo(*GetElementById("start")->firstChild()->GetLayoutObject());
  Vector<String> list;
  while (cursor) {
    list.push_back(ToDebugString(cursor));
    cursor.MoveToPreviousInlineLeafOnLine();
  }
  EXPECT_THAT(list, ElementsAre("DEF", "", "ABC"))
      << "We don't have items in first line.";
}

TEST_P(InlineCursorTest, PreviousLine) {
  InlineCursor cursor = SetupCursor("<div id=root>abc<br>xyz</div>");
  InlineCursor line1(cursor);
  while (line1 && !line1.Current().IsLineBox())
    line1.MoveToNext();
  ASSERT_TRUE(line1.IsNotNull());
  InlineCursor line2(line1);
  line2.MoveToNext();
  while (line2 && !line2.Current().IsLineBox())
    line2.MoveToNext();
  ASSERT_NE(line1, line2);

  InlineCursor should_be_null(line1);
  should_be_null.MoveToPreviousLine();
  EXPECT_TRUE(should_be_null.IsNull());

  InlineCursor should_be_line1(line2);
  should_be_line1.MoveToPreviousLine();
  EXPECT_EQ(line1, should_be_line1);
}

TEST_P(InlineCursorTest, CursorForDescendants) {
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
  InlineCursor cursor(*block_flow);
  EXPECT_TRUE(cursor.Current().IsLineBox());
  cursor.MoveToNext();
  EXPECT_TRUE(cursor.Current().IsText());
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

TEST_P(InlineCursorTest, MoveToVisualFirstOrLast) {
  SetBodyInnerHTML(R"HTML(
    <div id=root dir="rtl">
      here is
      <span id="span1">some <bdo dir="rtl">MIXED</bdo></span>
      <bdo dir="rtl">TEXT</bdo>
    </div>
  )HTML");

  //          _here_is_some_MIXED_TEXT_
  // visual:  _TXET_DEXIM_here_is_some_
  // in span:       ______        ____

  InlineCursor cursor1;
  cursor1.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("span1"));
  cursor1.MoveToVisualFirstForSameLayoutObject();
  EXPECT_EQ("FragmentItem Text \"MIXED\"", cursor1.Current()->ToString());

  InlineCursor cursor2;
  cursor2.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("span1"));
  cursor2.MoveToVisualLastForSameLayoutObject();
  EXPECT_EQ("FragmentItem Text \"some\"", cursor2.Current()->ToString());
}

class InlineCursorBlockFragmentationTest : public RenderingTest {};

TEST_F(InlineCursorBlockFragmentationTest, MoveToLayoutObject) {
  // This creates 3 columns, 1 line in each column.
  SetBodyInnerHTML(R"HTML(
    <style>
    #container {
      column-width: 6ch;
      font-family: monospace;
      font-size: 10px;
      height: 1.5em;
    }
    </style>
    <div id="container">
      <span id="span1">1111 22</span><span id="span2">33 4444</span>
    </div>
  )HTML");
  const LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  const LayoutObject* text1 = span1->SlowFirstChild();
  const LayoutObject* span2 = GetLayoutObjectByElementId("span2");
  const LayoutObject* text2 = span2->SlowFirstChild();

  // Enumerate all fragments for |LayoutText|.
  {
    InlineCursor cursor;
    cursor.MoveTo(*text1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
                ElementsAre("1111", "22"));
  }
  {
    InlineCursor cursor;
    cursor.MoveTo(*text2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
                ElementsAre("33", "4444"));
  }
  // |MoveTo| can find no fragments for culled inline.
  {
    InlineCursor cursor;
    cursor.MoveTo(*span1);
    EXPECT_FALSE(cursor);
  }
  {
    InlineCursor cursor;
    cursor.MoveTo(*span2);
    EXPECT_FALSE(cursor);
  }
  // But |MoveToIncludingCulledInline| should find its descendants.
  {
    InlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*span1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
                ElementsAre("1111", "22"));
  }
  {
    InlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*span2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
                ElementsAre("33", "4444"));
  }

  // Line-ranged cursors can find fragments only in the line.
  // The 1st line has "1111", from "text1".
  const LayoutBlockFlow* block_flow = span1->FragmentItemsContainer();
  InlineCursor cursor(*block_flow);
  EXPECT_TRUE(cursor.Current().IsLineBox());
  InlineCursor line1 = cursor.CursorForDescendants();
  const auto TestFragment1 = [&](const InlineCursor& initial_cursor) {
    InlineCursor cursor = initial_cursor;
    cursor.MoveTo(*text1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("1111"));
    cursor = initial_cursor;
    cursor.MoveToIncludingCulledInline(*span1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("1111"));
    cursor = initial_cursor;
    cursor.MoveTo(*text2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre());
    cursor = initial_cursor;
    cursor.MoveToIncludingCulledInline(*span2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre());
  };
  TestFragment1(line1);

  // The 2nd line has "22" from "text1" and "33" from text2.
  cursor.MoveToNextFragmentainer();
  EXPECT_TRUE(cursor);
  EXPECT_TRUE(cursor.Current().IsLineBox());
  InlineCursor line2 = cursor.CursorForDescendants();
  const auto TestFragment2 = [&](const InlineCursor& initial_cursor) {
    InlineCursor cursor = initial_cursor;
    cursor.MoveTo(*text1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("22"));
    cursor = initial_cursor;
    cursor.MoveToIncludingCulledInline(*span1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("22"));
    cursor = initial_cursor;
    cursor.MoveTo(*text2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("33"));
    cursor = initial_cursor;
    cursor.MoveToIncludingCulledInline(*span2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("33"));
  };
  TestFragment2(line2);

  // The 3rd line has "4444" from text2.
  cursor.MoveToNextFragmentainer();
  EXPECT_TRUE(cursor);
  EXPECT_TRUE(cursor.Current().IsLineBox());
  InlineCursor line3 = cursor.CursorForDescendants();
  const auto TestFragment3 = [&](const InlineCursor& initial_cursor) {
    InlineCursor cursor = initial_cursor;
    cursor.MoveTo(*text1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre());
    cursor = initial_cursor;
    cursor.MoveToIncludingCulledInline(*span1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre());
    cursor = initial_cursor;
    cursor.MoveTo(*text2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("4444"));
    cursor = initial_cursor;
    cursor.MoveToIncludingCulledInline(*span2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("4444"));
  };
  TestFragment3(line3);

  // Test cursors rooted at |FragmentItems|.
  // They can enumerate fragments only in the specified fragmentainer.
  HeapVector<Member<const PhysicalBoxFragment>> fragments;
  for (const PhysicalBoxFragment& fragment : block_flow->PhysicalFragments()) {
    DCHECK(fragment.HasItems());
    fragments.push_back(&fragment);
  }
  EXPECT_EQ(fragments.size(), 3u);
  TestFragment1(InlineCursor(*fragments[0], *fragments[0]->Items()));
  TestFragment2(InlineCursor(*fragments[1], *fragments[1]->Items()));
  TestFragment3(InlineCursor(*fragments[2], *fragments[2]->Items()));
}

}  // namespace

}  // namespace blink
