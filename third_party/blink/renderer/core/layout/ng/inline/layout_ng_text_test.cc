// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"

#include <numeric>
#include <sstream>
#include "build/build_config.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"

namespace blink {

class LayoutNGTextTest : public RenderingTest {
 protected:
  static constexpr unsigned kIncludeSnappedWidth = 1;

  std::string GetItemsAsString(const LayoutText& layout_text,
                               int num_glyphs = 0,
                               unsigned flags = 0) {
    if (layout_text.NeedsCollectInlines())
      return "LayoutText has NeedsCollectInlines";
    if (!layout_text.HasValidInlineItems())
      return "No valid inline items in LayoutText";
    const LayoutBlockFlow& block_flow = *layout_text.FragmentItemsContainer();
    if (block_flow.NeedsCollectInlines())
      return "LayoutBlockFlow has NeedsCollectInlines";
    const NGInlineNodeData& data = *block_flow.GetNGInlineNodeData();
    std::ostringstream stream;
    for (const NGInlineItem& item : data.items) {
      if (item.Type() != NGInlineItem::kText)
        continue;
      if (item.GetLayoutObject() == layout_text)
        stream << "*";
      stream << "{'"
             << data.text_content.Substring(item.StartOffset(), item.Length())
                    .Utf8()
             << "'";
      if (const auto* shape_result = item.TextShapeResult()) {
        stream << ", ShapeResult=" << shape_result->StartIndex() << "+"
               << shape_result->NumCharacters();
#if BUILDFLAG(IS_WIN)
        if (shape_result->NumCharacters() != shape_result->NumGlyphs())
          stream << " #glyphs=" << shape_result->NumGlyphs();
#else
        // Note: |num_glyphs| depends on installed font, we check only for
        // Windows because most of failures are reported on Windows.
        if (num_glyphs)
          stream << " #glyphs=" << num_glyphs;
#endif
        if (flags & kIncludeSnappedWidth)
          stream << " width=" << shape_result->SnappedWidth();
      }
      stream << "}" << std::endl;
    }
    return stream.str();
  }

  unsigned CountNumberOfGlyphs(const LayoutText& layout_text) {
    auto* const items = layout_text.GetNGInlineItems();
    return std::accumulate(items->begin(), items->end(), 0u,
                           [](unsigned sum, const NGInlineItem& item) {
                             return sum + item.TextShapeResult()->NumGlyphs();
                           });
  }
};

TEST_F(LayoutNGTextTest, SetTextWithOffsetAppendBidi) {
  SetBodyInnerHTML(u"<div dir=rtl id=target>\u05D0\u05D1\u05BC\u05D2</div>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.appendData(u"\u05D0\u05D1\u05BC\u05D2");

  EXPECT_EQ(
      "*{'\u05D0\u05D1\u05BC\u05D2\u05D0\u05D1\u05BC\u05D2', "
      "ShapeResult=0+8 #glyphs=6}\n",
      GetItemsAsString(*text.GetLayoutObject(), 6));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetAppendControl) {
  SetBodyInnerHTML(u"<pre id=target>a</pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  // Note: "\n" is control character instead of text character.
  text.appendData("\nX");

  EXPECT_EQ(
      "*{'a', ShapeResult=0+1}\n"
      "*{'X', ShapeResult=2+1}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetAppendCollapseWhiteSpace) {
  SetBodyInnerHTML(u"<p id=target>abc </p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.appendData("XYZ");

  EXPECT_EQ("*{'abc XYZ', ShapeResult=0+7}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetAppend) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.appendData("xyz");

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XYZxyz', ShapeResult=3+6}\n"
      "{'def', ShapeResult=9+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1213235
TEST_F(LayoutNGTextTest, SetTextWithOffsetAppendEmojiWithZWJ) {
  // Compose "Woman Shrugging"
  //    U+1F937 Shrug (U+D83E U+0xDD37)
  //    U+200D  ZWJ
  //    U+2640  Female Sign
  //    U+FE0F  Variation Selector-16
  SetBodyInnerHTML(
      u"<pre id=target>&#x1F937;</pre>"
      "<p id=checker>&#x1F937;&#x200D;&#x2640;&#xFE0F</p>");

  // Check whether we have "Woman Shrug glyph or not.
  const auto& checker = *To<LayoutText>(
      GetElementById("checker")->firstChild()->GetLayoutObject());
  if (CountNumberOfGlyphs(checker) != 1)
    return;

  Text& text = To<Text>(*GetElementById("target")->firstChild());
  UpdateAllLifecyclePhasesForTest();
  text.appendData(u"\u200D");
  EXPECT_EQ("*{'\U0001F937\u200D', ShapeResult=0+3 #glyphs=2}\n",
            GetItemsAsString(*text.GetLayoutObject(), 2));

  UpdateAllLifecyclePhasesForTest();
  text.appendData(u"\u2640");
  EXPECT_EQ("*{'\U0001F937\u200D\u2640', ShapeResult=0+4 #glyphs=1}\n",
            GetItemsAsString(*text.GetLayoutObject(), 1));

  UpdateAllLifecyclePhasesForTest();
  text.appendData(u"\uFE0F");
  EXPECT_EQ("*{'\U0001F937\u200D\u2640\uFE0F', ShapeResult=0+5 #glyphs=1}\n",
            GetItemsAsString(*text.GetLayoutObject(), 1));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetDelete) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>xXYZyz<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.deleteData(1, 3, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'xyz', ShapeResult=3+3}\n"
      "{'def', ShapeResult=6+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteCollapseWhiteSpace) {
  SetBodyInnerHTML(u"<p id=target>ab  XY  cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(4, 2, ASSERT_NO_EXCEPTION);  // remove "XY"

  EXPECT_EQ("*{'ab cd', ShapeResult=0+5}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteCollapseWhiteSpaceEnd) {
  SetBodyInnerHTML(u"<p id=target>a bc</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(2, 2, ASSERT_NO_EXCEPTION);  // remove "bc"

  EXPECT_EQ("*{'a', ShapeResult=0+1}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1253931
TEST_F(LayoutNGTextTest, SetTextWithOffsetCopyItemBefore) {
  SetBodyInnerHTML(u"<p id=target><img> a</p>");

  auto& target = *GetElementById("target");
  const auto& text = *To<Text>(target.lastChild());

  target.appendChild(Text::Create(GetDocument(), "YuGFkVSKiG"));
  UpdateAllLifecyclePhasesForTest();

  // Combine Text nodes "a " and "YuGFkVSKiG".
  target.normalize();
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ("*{' aYuGFkVSKiG', ShapeResult=1+12}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

// web_tests/external/wpt/editing/run/delete.html?993-993
// web_tests/external/wpt/editing/run/forwarddelete.html?1193-1193
TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteNbspInPreWrap) {
  InsertStyleElement("#target { white-space:pre-wrap; }");
  SetBodyInnerHTML(u"<p id=target>&nbsp; abc</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(0, 1, ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "*{' ', ShapeResult=0+1}\n"
      "*{'abc', ShapeResult=2+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteRTL) {
  SetBodyInnerHTML(u"<p id=target dir=rtl>0 234</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(2, 2, ASSERT_NO_EXCEPTION);  // remove "23"

  EXPECT_EQ(
      "*{'0', ShapeResult=0+1}\n"
      "*{' ', ShapeResult=1+1}\n"
      "*{'4', ShapeResult=2+1}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1000685
TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteRTL2) {
  SetBodyInnerHTML(u"<p id=target dir=rtl>0(xy)5</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(0, 1, ASSERT_NO_EXCEPTION);  // remove "0"

  EXPECT_EQ(
      "*{'(', ShapeResult=0+1}\n"
      "*{'xy', ShapeResult=1+2}\n"
      "*{')', ShapeResult=3+1}\n"
      "*{'5', ShapeResult=4+1}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// editing/deleting/delete_ws_fixup.html
TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteThenNonCollapse) {
  SetBodyInnerHTML(u"<div id=target>abc def<b> </b>ghi</div>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(4, 3, ASSERT_NO_EXCEPTION);  // remove "def"

  EXPECT_EQ(
      "*{'abc ', ShapeResult=0+4}\n"
      "{''}\n"
      "{'ghi', ShapeResult=4+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// editing/deleting/delete_ws_fixup.html
TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteThenNonCollapse2) {
  SetBodyInnerHTML(u"<div id=target>abc def<b> X </b>ghi</div>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(4, 3, ASSERT_NO_EXCEPTION);  // remove "def"

  EXPECT_EQ(
      "*{'abc ', ShapeResult=0+4}\n"
      "{'X ', ShapeResult=4+2}\n"
      "{'ghi', ShapeResult=6+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1039143
TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteWithBidiControl) {
  // In text content, we have bidi control codes:
  // U+2066 U+2069 \n U+2066 abc U+2066
  SetBodyInnerHTML(u"<pre><b id=target dir=ltr>\nabc</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(0, 1, ASSERT_NO_EXCEPTION);  // remove "\n"

  EXPECT_EQ("LayoutText has NeedsCollectInlines",
            GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1125262
TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteWithGeneratedBreakOpportunity) {
  InsertStyleElement("#target { white-space:nowrap; }");
  SetBodyInnerHTML(u"<p><b><i id=target>ab\n</i>\n</b>\n</div>");
  // We have two ZWS for "</i>\n" and "</b>\n".
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(2, 1, ASSERT_NO_EXCEPTION);  // remove "\n"

  EXPECT_EQ(
      "*{'ab', ShapeResult=0+2}\n"
      "{''}\n"
      "{''}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1123251
TEST_F(LayoutNGTextTest, SetTextWithOffsetEditingTextCollapsedSpace) {
  SetBodyInnerHTML(u"<p id=target></p>");
  // Simulate: insertText("A") + InsertHTML("X ")
  Text& text = *GetDocument().CreateEditingTextNode("AX ");
  GetElementById("target")->appendChild(&text);
  UpdateAllLifecyclePhasesForTest();

  text.replaceData(0, 2, " ", ASSERT_NO_EXCEPTION);

  EXPECT_EQ("*{''}\n", GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetInsert) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.insertData(1, "xyz", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XxyzYZ', ShapeResult=3+6}\n"
      "{'def', ShapeResult=9+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetInsertAfterSpace) {
  SetBodyInnerHTML(u"<p id=target>ab cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.insertData(3, " XYZ ", ASSERT_NO_EXCEPTION);

  EXPECT_EQ("*{'ab XYZ cd', ShapeResult=0+9}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetInserBeforetSpace) {
  SetBodyInnerHTML(u"<p id=target>ab cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.insertData(2, " XYZ ", ASSERT_NO_EXCEPTION);

  EXPECT_EQ("*{'ab XYZ cd', ShapeResult=0+9}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

// https://crbug.com/1391668
TEST_F(LayoutNGTextTest, SetTextWithOffsetInsertSameCharacters) {
  LoadAhem();
  InsertStyleElement("body { font: 10px/15px Ahem; } b { font-size: 50px; }");
  SetBodyInnerHTML(u"<p><b id=target>a</b>aa</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.insertData(0, "aa", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "*{'aaa', ShapeResult=0+3 width=\"150\"}\n"
      "{'aa', ShapeResult=3+2 width=\"20\"}\n",
      GetItemsAsString(*text.GetLayoutObject(), 0, kIncludeSnappedWidth));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetNoRelocation) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  // Note: |CharacterData::setData()| is implementation of Node::setNodeValue()
  // for |CharacterData|.
  text.setData("xyz");

  EXPECT_EQ("LayoutText has NeedsCollectInlines",
            GetItemsAsString(*text.GetLayoutObject()))
      << "There are no optimization for setData()";
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetPrepend) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.insertData(1, "xyz", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XxyzYZ', ShapeResult=3+6}\n"
      "{'def', ShapeResult=9+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetReplace) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZW<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.replaceData(1, 2, "yz", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XyzW', ShapeResult=3+4}\n"
      "{'def', ShapeResult=7+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetReplaceCollapseWhiteSpace) {
  SetBodyInnerHTML(u"<p id=target>ab  XY  cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.replaceData(4, 2, " ", ASSERT_NO_EXCEPTION);  // replace "XY" to " "

  EXPECT_EQ("*{'ab cd', ShapeResult=0+5}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetReplaceToExtend) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZW<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.replaceData(1, 2, "xyz", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XxyzW', ShapeResult=3+5}\n"
      "{'def', ShapeResult=8+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetReplaceToShrink) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZW<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.replaceData(1, 2, "y", ASSERT_NO_EXCEPTION);

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XyW', ShapeResult=3+3}\n"
      "{'def', ShapeResult=6+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetToEmpty) {
  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  // Note: |CharacterData::setData()| is implementation of Node::setNodeValue()
  // for |CharacterData|.
  // Note: |setData()| detaches layout object from |Text| node since
  // |Text::TextLayoutObjectIsNeeded()| returns false for empty text.
  text.setData("");
  UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(nullptr, text.GetLayoutObject());
}

}  // namespace blink
