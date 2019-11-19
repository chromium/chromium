// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/layout_ng_text.h"

#include <sstream>
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node_data.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"

namespace blink {

class LayoutNGTextTest : public PageTestBase {
 protected:
  std::string GetItemsAsString(const LayoutText& layout_text) {
    if (layout_text.NeedsCollectInlines())
      return "LayoutText has NeedsCollectInlines";
    if (!layout_text.HasValidInlineItems())
      return "No valid inline items in LayoutText";
    const LayoutBlockFlow& block_flow = *layout_text.ContainingNGBlockFlow();
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
      if (item.TextShapeResult()) {
        stream << ", ShapeResult=" << item.TextShapeResult()->StartIndex()
               << "+" << item.TextShapeResult()->NumCharacters();
      }
      stream << "}" << std::endl;
    }
    return stream.str();
  }
};

TEST_F(LayoutNGTextTest, SetTextWithOffsetAppendControl) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(u"<p id=target>abc </p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.appendData("XYZ");

  EXPECT_EQ("*{'abc XYZ', ShapeResult=0+7}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetAppend) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(u"<pre id=target><a>abc</a>XYZ<b>def</b></pre>");
  Text& text = To<Text>(*GetElementById("target")->firstChild()->nextSibling());
  text.appendData("xyz");

  EXPECT_EQ(
      "{'abc', ShapeResult=0+3}\n"
      "*{'XYZxyz', ShapeResult=3+6}\n"
      "{'def', ShapeResult=9+3}\n",
      GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetDelete) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(u"<p id=target>ab  XY  cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(4, 2, ASSERT_NO_EXCEPTION);  // remove "XY"

  EXPECT_EQ("*{'ab cd', ShapeResult=0+5}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteCollapseWhiteSpaceEnd) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(u"<p id=target>a bc</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(2, 2, ASSERT_NO_EXCEPTION);  // remove "bc"

  EXPECT_EQ("*{'a', ShapeResult=0+1}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteRTL) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(u"<p id=target dir=rtl>0 234</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(2, 2, ASSERT_NO_EXCEPTION);  // remove "23"

  EXPECT_EQ("*{'0 4', ShapeResult=0+3}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

// http://crbug.com/1000685
TEST_F(LayoutNGTextTest, SetTextWithOffsetDeleteRTL2) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(u"<p id=target dir=rtl>0(xy)5</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.deleteData(0, 1, ASSERT_NO_EXCEPTION);  // remove "0"

  EXPECT_EQ("*{'(xy)5', ShapeResult=0+5}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetInsert) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(u"<p id=target>ab cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.insertData(3, " XYZ ", ASSERT_NO_EXCEPTION);

  EXPECT_EQ("*{'ab XYZ cd', ShapeResult=0+9}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetInserBeforetSpace) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(u"<p id=target>ab cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.insertData(2, " XYZ ", ASSERT_NO_EXCEPTION);

  EXPECT_EQ("*{'ab XYZ cd', ShapeResult=0+9}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetNoRelocation) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

  SetBodyInnerHTML(u"<p id=target>ab  XY  cd</p>");
  Text& text = To<Text>(*GetElementById("target")->firstChild());
  text.replaceData(4, 2, " ", ASSERT_NO_EXCEPTION);  // replace "XY" to " "

  EXPECT_EQ("*{'ab cd', ShapeResult=0+5}\n",
            GetItemsAsString(*text.GetLayoutObject()));
}

TEST_F(LayoutNGTextTest, SetTextWithOffsetReplaceToExtend) {
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
  if (!RuntimeEnabledFeatures::LayoutNGEnabled())
    return;

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
