// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_test.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

namespace {

using ::testing::ElementsAre;

String ToDebugString(const NGInlineCursor& cursor) {
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

Vector<String> LayoutObjectToDebugStringList(NGInlineCursor cursor) {
  Vector<String> list;
  for (; cursor; cursor.MoveToNextForSameLayoutObject())
    list.push_back(ToDebugString(cursor));
  return list;
}

class NGInlineCursorTest : public NGLayoutTest,
                           public testing::WithParamInterface<bool> {
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

  Vector<String> SiblingsToDebugStringList(const NGInlineCursor& start) {
    Vector<String> list;
    for (NGInlineCursor cursor(start); cursor;
         cursor.MoveToNextSkippingChildren())
      list.push_back(ToDebugString(cursor));
    return list;
  }

  // Test |MoveToNextSibling| and |NGInlineBackwardCursor| return the same
  // instances, except that the order is reversed.
  void TestPrevoiusSibling(const NGInlineCursor& start) {
    DCHECK(start.HasRoot());
    Vector<const NGFragmentItem*> forwards;
    for (NGInlineCursor cursor(start); cursor;
         cursor.MoveToNextSkippingChildren())
      forwards.push_back(cursor.CurrentItem());
    Vector<const NGFragmentItem*> backwards;
    for (NGInlineBackwardCursor cursor(start); cursor;
         cursor.MoveToPreviousSibling())
      backwards.push_back(cursor.Current().Item());
    backwards.Reverse();
    EXPECT_THAT(backwards, forwards);
  }

  Vector<String> ToDebugStringListWithBidiLevel(const NGInlineCursor& start) {
    Vector<String> list;
    for (NGInlineCursor cursor(start); cursor; cursor.MoveToNext()) {
      // Inline boxes do not have bidi level.
      if (cursor.Current().IsInlineBox())
        continue;
      list.push_back(ToDebugStringWithBidiLevel(cursor));
    }
    return list;
  }

  String ToDebugStringWithBidiLevel(const NGInlineCursor& cursor) {
    if (!cursor.Current().IsText() && !cursor.Current().IsAtomicInline())
      return ToDebugString(cursor);
    StringBuilder result;
    result.Append(ToDebugString(cursor));
    result.Append(':');
    result.AppendNumber(cursor.Current().BidiLevel());
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
  EXPECT_THAT(list,
              ElementsAre("#linebox", "abc:0", "#def:0", "#ghi:1", "jkl:0"));
}

TEST_P(NGInlineCursorTest, BidiLevelInlineBoxRTL) {
  InsertStyleElement("b { display: inline-block; }");
  NGInlineCursor cursor = SetupCursor(
      "<div id=root dir=rtl>"
      "abc<b id=def>def</b><bdo dir=rtl><b id=ghi>GHI</b></bdo>jkl</div>");
  Vector<String> list = ToDebugStringListWithBidiLevel(cursor);
  EXPECT_THAT(list,
              ElementsAre("#linebox", "#ghi:3", "jkl:2", "#def:1", "abc:2"));
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
  ASSERT_TRUE(line.Current().IsLineBox()) << line;
  NGInlineCursor cursor = line.CursorForDescendants();
  EXPECT_EQ(line.GetLayoutBlockFlow(), cursor.GetLayoutBlockFlow());
}

TEST_P(NGInlineCursorTest, Parent) {
  NGInlineCursor cursor = SetupCursor(R"HTML(
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

TEST_P(NGInlineCursorTest, ContainingLine) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a id=target>def</a>ghi<br>xyz</div>");
  const LayoutBlockFlow& block_flow = *cursor.GetLayoutBlockFlow();
  NGInlineCursor line1(cursor);
  ASSERT_TRUE(line1.Current().IsLineBox());

  NGInlineCursor line2(line1);
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

TEST_P(NGInlineCursorTest, CulledInlineWithAtomicInline) {
  SetBodyInnerHTML(
      "<div id=root>"
      "<b id=culled>abc<div style=display:inline>ABC<br>XYZ</div>xyz</b>"
      "</div>");
  NGInlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
              ElementsAre("abc", "ABC", "", "XYZ", "xyz"));
}

// We should not have float:right fragment, because it isn't in-flow in
// an inline formatting context.
// For https://crbug.com/1026022
TEST_P(NGInlineCursorTest, CulledInlineWithFloat) {
  SetBodyInnerHTML(
      "<div id=root>"
      "<b id=culled>abc<div style=float:right></div>xyz</b>"
      "</div>");
  NGInlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("abc", "xyz"));
}

TEST_P(NGInlineCursorTest, CulledInlineWithOOF) {
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      <b id=culled>abc<span style="position:absolute"></span>xyz</b>
    </div>
  )HTML");
  NGInlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("abc", "xyz"));
}

TEST_P(NGInlineCursorTest, CulledInlineNested) {
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      <b id=culled><span>abc</span> xyz</b>
    </div>
  )HTML");
  NGInlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("abc", "xyz"));
}

TEST_P(NGInlineCursorTest, CulledInlineBlockChild) {
  SetBodyInnerHTML(R"HTML(
    <div id=root>
      <b id=culled>
        <div>block</div>
        <span>abc</span> xyz
      </b>
    </div>
  )HTML");
  NGInlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("culled"));
  if (RuntimeEnabledFeatures::LayoutNGBlockInInlineEnabled()) {
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
                ElementsAre("#culled", "#culled", "#culled"));
  } else {
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor), ElementsAre("#culled"));
  }
}

TEST_P(NGInlineCursorTest, CulledInlineWithRoot) {
  NGInlineCursor cursor = SetupCursor(R"HTML(
    <div id="root"><a id="a"><b>abc</b><br><i>xyz</i></a></div>
  )HTML");
  const LayoutObject* layout_inline_a = GetLayoutObjectByElementId("a");
  cursor.MoveToIncludingCulledInline(*layout_inline_a);
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
              ElementsAre("abc", "", "xyz"));
}

TEST_P(NGInlineCursorTest, CulledInlineWithoutRoot) {
  SetBodyInnerHTML(R"HTML(
    <div id="root"><a id="a"><b>abc</b><br><i>xyz</i></a></div>
  )HTML");
  const LayoutObject* layout_inline_a = GetLayoutObjectByElementId("a");
  NGInlineCursor cursor;
  cursor.MoveToIncludingCulledInline(*layout_inline_a);
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
              ElementsAre("abc", "", "xyz"));
}

TEST_P(NGInlineCursorTest, CursorForMovingAcrossFragmentainer) {
  RuntimeEnabledFeaturesTestHelpers::ScopedLayoutNGBlockFragmentation
      block_fragmentation(true);
  LoadAhem();
  InsertStyleElement(
      "div { font: 10px/15px Ahem; column-count: 2; width: 20ch; }");
  SetBodyInnerHTML("<div id=m>abc<br>def<br><b id=t>ghi</b><br>jkl<br></div>");
  // The HTML is rendered as:
  //    abc ghi
  //    def jkl

  // MoveTo(LayoutObject) makes |NGInlineCursor| to be able to move across
  // fragmentainer.
  NGInlineCursor cursor;
  cursor.MoveTo(*GetElementById("t")->firstChild()->GetLayoutObject());
  ASSERT_TRUE(cursor.IsBlockFragmented()) << cursor;

  NGInlineCursor cursor2(cursor.ContainerFragment());
  ASSERT_FALSE(cursor2.IsBlockFragmented()) << cursor2;
  cursor2.MoveTo(*cursor.CurrentItem());
  ASSERT_FALSE(cursor2.IsBlockFragmented());

  NGInlineCursor cursor3 = cursor2.CursorForMovingAcrossFragmentainer();
  EXPECT_TRUE(cursor3.IsBlockFragmented()) << cursor3;
  EXPECT_EQ(&cursor2.ContainerFragment(), &cursor3.ContainerFragment());
  EXPECT_EQ(cursor2.CurrentItem(), cursor3.CurrentItem());
}

TEST_P(NGInlineCursorTest, FirstChild) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToFirstChild();
  EXPECT_EQ("abc", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryMoveToFirstChild());
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
  EXPECT_FALSE(cursor.TryMoveToFirstChild());
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

// http://crbug.com/1295087
TEST_P(NGInlineCursorTest, FirstNonPseudoLeafWithBlockImage) {
  InsertStyleElement("img { display: block; }");
  NGInlineCursor cursor = SetupCursor("<p id=root><b><img id=target></b></p>");

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

TEST_P(NGInlineCursorTest, IsEmptyLineBox) {
  InsertStyleElement("b { margin-bottom: 1px; }");
  NGInlineCursor cursor = SetupCursor("<div id=root>abc<br><b></b></div>");

  EXPECT_FALSE(cursor.Current().IsEmptyLineBox())
      << "'abc\\n' is in non-empty line box.";
  cursor.MoveToNextLine();
  EXPECT_TRUE(cursor.Current().IsEmptyLineBox())
      << "<b></b> with margin produces empty line box.";
}

TEST_P(NGInlineCursorTest, LastChild) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  cursor.MoveToLastChild();
  EXPECT_EQ("xyz", ToDebugString(cursor));
  EXPECT_FALSE(cursor.TryMoveToLastChild());
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
  EXPECT_FALSE(cursor.TryMoveToLastChild());
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

TEST_P(NGInlineCursorTest, NextIncludingFragmentainer) {
  RuntimeEnabledFeaturesTestHelpers::ScopedLayoutNGBlockFragmentation
      block_fragmentation(true);
  // TDOO(yosin): Remove style for <b> once NGFragmentItem don't do culled
  // inline.
  LoadAhem();
  InsertStyleElement(
      "b { background: gray; }"
      "div { font: 10px/15px Ahem; column-count: 2; width: 20ch; }");
  SetBodyInnerHTML("<div id=m>abc<br>def<br><b>ghi</b><br>jkl</div>");
  NGInlineCursor cursor;
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

TEST_P(NGInlineCursorTest, NextWithEllipsisInlineBoxOnly) {
  LoadAhem();
  InsertStyleElement(
      "#root {"
      "font: 10px/1 Ahem;"
      "width: 5ch;"
      "overflow: hidden;"
      "text-overflow: ellipsis;"
      "}"
      "span { border: solid 10ch blue; }");
  NGInlineCursor cursor = SetupCursor("<div id=root><span></span></div>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("#linebox", "LayoutInline SPAN"));
}

TEST_P(NGInlineCursorTest, NextWithListItem) {
  NGInlineCursor cursor = SetupCursor("<ul><li id=root>abc</li></ul>");
  Vector<String> list = ToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("LayoutNGOutsideListMarker ::marker",
                                "#linebox", "abc"));
  EXPECT_EQ(GetLayoutObjectByElementId("root"), cursor.GetLayoutBlockFlow());
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

// Note: This is for AccessibilityLayoutTest.NextOnLine.
TEST_P(NGInlineCursorTest, NextInlineLeafOnLineFromLayoutInline) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
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

TEST_P(NGInlineCursorTest, NextInlineLeafOnLineFromNestedLayoutInline) {
  // Never return a descendant for AXLayoutObject::NextOnLine().
  // Instead, if NextOnLine() is called on a container, return the first
  // content from a sibling subtree.
  NGInlineCursor cursor = SetupCursor(
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

TEST_P(NGInlineCursorTest, NextInlineLeafOnLineFromLayoutText) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
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
  while (line1 && !line1.Current().IsLineBox())
    line1.MoveToNext();
  ASSERT_TRUE(line1.IsNotNull());
  NGInlineCursor line2(line1);
  line2.MoveToNext();
  while (line2 && !line2.Current().IsLineBox())
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

  NGInlineCursor cursor2;
  cursor2.MoveTo(*GetElementById("ib")->firstChild()->GetLayoutObject());
  EXPECT_EQ(GetLayoutObjectByElementId("ib"), cursor2.GetLayoutBlockFlow());
}

TEST_P(NGInlineCursorTest, NextForSameLayoutObject) {
  NGInlineCursor cursor = SetupCursor("<pre id=root>abc\ndef\nghi</pre>");
  cursor.MoveTo(*GetLayoutObjectByElementId("root")->SlowFirstChild());
  EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
              ElementsAre("abc", "", "def", "", "ghi"));
}

// Test |NextForSameLayoutObject| with limit range set.
TEST_P(NGInlineCursorTest, NextForSameLayoutObjectWithRange) {
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
  NGInlineCursor cursor(*root);
  cursor.MoveToFirstLine();
  cursor.MoveToNextLine();
  NGInlineCursor line2 = cursor.CursorForDescendants();

  // Now |line2| is limited to the 2nd line. There should be only one framgnet
  // for `<span>` if we search using `line2`.
  LayoutObject* span1 = GetLayoutObjectByElementId("span1");
  line2.MoveTo(*span1);
  EXPECT_THAT(LayoutObjectToDebugStringList(line2), ElementsAre("#span1"));
}

TEST_P(NGInlineCursorTest, Sibling) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root>abc<a>DEF<b>GHI</b></a>xyz</div>");
  TestPrevoiusSibling(cursor.CursorForDescendants());
  cursor.MoveToFirstChild();  // go to "abc"
  Vector<String> list = SiblingsToDebugStringList(cursor);
  EXPECT_THAT(list, ElementsAre("abc", "LayoutInline A", "xyz"));
}

TEST_P(NGInlineCursorTest, Sibling2) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("a, b { background: gray; }");
  NGInlineCursor cursor =
      SetupCursor("<div id=root><a>abc<b>def</b>xyz</a></div>");
  cursor.MoveToFirstChild();  // go to <a>abc</a>
  TestPrevoiusSibling(cursor.CursorForDescendants());
  cursor.MoveToFirstChild();  // go to "abc"
  Vector<String> list = SiblingsToDebugStringList(cursor);
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

TEST_P(NGInlineCursorTest, PositionForPointInChildHorizontalLTR) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "direction: ltr;"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: horizontal-tb;"
      "}");
  NGInlineCursor cursor = SetupCursor("<p id=root>ab</p>");
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

TEST_P(NGInlineCursorTest, PositionForPointInChildHorizontalRTL) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "direction: rtl;"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: horizontal-tb;"
      "}");
  NGInlineCursor cursor = SetupCursor("<p id=root><bdo dir=rtl>AB</bdo></p>");
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

TEST_P(NGInlineCursorTest, PositionForPointInChildVerticalLTR) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "direction: ltr;"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: vertical-lr;"
      "}");
  NGInlineCursor cursor = SetupCursor("<p id=root>ab</p>");
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

TEST_P(NGInlineCursorTest, PositionForPointInChildVerticalRTL) {
  LoadAhem();
  InsertStyleElement(
      "p {"
      "direction: rtl;"
      "font: 10px/20px Ahem;"
      "padding: 10px;"
      "writing-mode: vertical-rl;"
      "}");
  NGInlineCursor cursor = SetupCursor("<p id=root><bdo dir=rtl>AB</bdo></p>");
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
TEST_P(NGInlineCursorTest, PositionForPointInChildBlockChildren) {
  InsertStyleElement("b { display: inline-block; }");
  // Note: <b>.ChildrenInline() == false
  NGInlineCursor cursor =
      SetupCursor("<div id=root>a<b id=target><div>x</div></b></div>");
  const Element& target = *GetElementById("target");
  cursor.MoveTo(*target.GetLayoutObject());
  EXPECT_EQ(PositionWithAffinity(Position::FirstPositionInNode(target)),
            cursor.PositionForPointInChild(PhysicalOffset()));
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

TEST_P(NGInlineCursorTest, PreviousIncludingFragmentainer) {
  RuntimeEnabledFeaturesTestHelpers::ScopedLayoutNGBlockFragmentation
      block_fragmentation(true);
  // TDOO(yosin): Remove style for <b> once NGFragmentItem don't do culled
  // inline.
  LoadAhem();
  InsertStyleElement(
      "b { background: gray; }"
      "div { font: 10px/15px Ahem; column-count: 2; width: 20ch; }");
  SetBodyInnerHTML("<div id=m>abc<br>def<br><b>ghi</b><br>jkl</div>");
  NGInlineCursor cursor;
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

TEST_P(NGInlineCursorTest, PreviousInlineLeafOnLineFromLayoutInline) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
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

TEST_P(NGInlineCursorTest, PreviousInlineLeafOnLineFromNestedLayoutInline) {
  // Never return a descendant for AXLayoutObject::PreviousOnLine().
  // Instead, if PreviousOnLine() is called on a container, return a previpus
  // item from the previous siblings subtree.
  NGInlineCursor cursor = SetupCursor(
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

TEST_P(NGInlineCursorTest, PreviousInlineLeafOnLineFromLayoutText) {
  // TDOO(yosin): Remove <style> once NGFragmentItem don't do culled inline.
  InsertStyleElement("b { background: gray; }");
  NGInlineCursor cursor = SetupCursor(
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

TEST_P(NGInlineCursorTest, PreviousLine) {
  NGInlineCursor cursor = SetupCursor("<div id=root>abc<br>xyz</div>");
  NGInlineCursor line1(cursor);
  while (line1 && !line1.Current().IsLineBox())
    line1.MoveToNext();
  ASSERT_TRUE(line1.IsNotNull());
  NGInlineCursor line2(line1);
  line2.MoveToNext();
  while (line2 && !line2.Current().IsLineBox())
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

TEST_P(NGInlineCursorTest, MoveToVisualFirstOrLast) {
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

  NGInlineCursor cursor1;
  cursor1.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("span1"));
  cursor1.MoveToVisualFirstForSameLayoutObject();
  EXPECT_EQ("NGPhysicalTextFragment 'MIXED'", cursor1.Current()->ToString());

  NGInlineCursor cursor2;
  cursor2.MoveToIncludingCulledInline(*GetLayoutObjectByElementId("span1"));
  cursor2.MoveToVisualLastForSameLayoutObject();
  EXPECT_EQ("NGPhysicalTextFragment 'some'", cursor2.Current()->ToString());
}

class NGInlineCursorBlockFragmentationTest
    : public NGLayoutTest,
      private ScopedLayoutNGBlockFragmentationForTest {
 public:
  NGInlineCursorBlockFragmentationTest()
      : ScopedLayoutNGBlockFragmentationForTest(true) {}
};

TEST_F(NGInlineCursorBlockFragmentationTest, MoveToLayoutObject) {
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
    NGInlineCursor cursor;
    cursor.MoveTo(*text1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
                ElementsAre("1111", "22"));
  }
  {
    NGInlineCursor cursor;
    cursor.MoveTo(*text2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
                ElementsAre("33", "4444"));
  }
  // |MoveTo| can find no fragments for culled inline.
  {
    NGInlineCursor cursor;
    cursor.MoveTo(*span1);
    EXPECT_FALSE(cursor);
  }
  {
    NGInlineCursor cursor;
    cursor.MoveTo(*span2);
    EXPECT_FALSE(cursor);
  }
  // But |MoveToIncludingCulledInline| should find its descendants.
  {
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*span1);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
                ElementsAre("1111", "22"));
  }
  {
    NGInlineCursor cursor;
    cursor.MoveToIncludingCulledInline(*span2);
    EXPECT_THAT(LayoutObjectToDebugStringList(cursor),
                ElementsAre("33", "4444"));
  }

  // Line-ranged cursors can find fragments only in the line.
  // The 1st line has "1111", from "text1".
  const LayoutBlockFlow* block_flow = span1->FragmentItemsContainer();
  NGInlineCursor cursor(*block_flow);
  EXPECT_TRUE(cursor.Current().IsLineBox());
  NGInlineCursor line1 = cursor.CursorForDescendants();
  const auto TestFragment1 = [&](const NGInlineCursor& initial_cursor) {
    NGInlineCursor cursor = initial_cursor;
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
  NGInlineCursor line2 = cursor.CursorForDescendants();
  const auto TestFragment2 = [&](const NGInlineCursor& initial_cursor) {
    NGInlineCursor cursor = initial_cursor;
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
  NGInlineCursor line3 = cursor.CursorForDescendants();
  const auto TestFragment3 = [&](const NGInlineCursor& initial_cursor) {
    NGInlineCursor cursor = initial_cursor;
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

  // Test cursors rooted at |NGFragmentItems|.
  // They can enumerate fragments only in the specified fragmentainer.
  HeapVector<Member<const NGPhysicalBoxFragment>> fragments;
  for (const NGPhysicalBoxFragment& fragment :
       block_flow->PhysicalFragments()) {
    DCHECK(fragment.HasItems());
    fragments.push_back(&fragment);
  }
  EXPECT_EQ(fragments.size(), 3u);
  TestFragment1(NGInlineCursor(*fragments[0], *fragments[0]->Items()));
  TestFragment2(NGInlineCursor(*fragments[1], *fragments[1]->Items()));
  TestFragment3(NGInlineCursor(*fragments[2], *fragments[2]->Items()));
}

}  // namespace

}  // namespace blink
