// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"

namespace blink {

class PlainTextNodeTest : public testing::Test {
 public:
  static std::pair<String, bool> NormalizeSpacesAndMaybeBidi(
      StringView text,
      bool normalize_space = false) {
    return PlainTextNode::NormalizeSpacesAndMaybeBidi(text, normalize_space);
  }

  static Font& TestFont() {
    return *MakeGarbageCollected<Font>(FontDescription{}, nullptr);
  }

  static PlainTextNode& CreatePlainTextNode(const TextRun& run,
                                            bool normalize_space,
                                            bool supports_bidi) {
    return *MakeGarbageCollected<PlainTextNode>(
        run, normalize_space, TestFont(), supports_bidi, nullptr);
  }

  static constexpr bool kDirectionalOverride = true;
  static constexpr bool kNormalizeSpace = true;
  static constexpr bool kSupportsBidi = true;
};

TEST_F(PlainTextNodeTest, NormalizeSpacesAndMaybeBidiNoConversion) {
  {
    String source8("foo");
    ASSERT_TRUE(source8.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source8);
    EXPECT_EQ(source8.Impl(), normalized.Impl());
    EXPECT_FALSE(is_bidi);
  }
  {
    String source16(u"foo");
    ASSERT_FALSE(source16.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source16);
    EXPECT_EQ(source16.Impl(), normalized.Impl());
    EXPECT_FALSE(is_bidi);
  }
  {
    String source16(u"foo\u0590");
    ASSERT_FALSE(source16.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source16);
    EXPECT_EQ(source16.Impl(), normalized.Impl());
    EXPECT_TRUE(is_bidi);
  }
  {
    String source8("fo o\xA0");
    ASSERT_TRUE(source8.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source8);
    EXPECT_EQ(source8.Impl(), normalized.Impl());
    EXPECT_FALSE(is_bidi);
  }
  {
    String source16(u"fo o\u00A0");
    ASSERT_FALSE(source16.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source16);
    EXPECT_EQ(source16.Impl(), normalized.Impl());
    EXPECT_FALSE(is_bidi);
  }
}

TEST_F(PlainTextNodeTest, NormalizeSpacesAndMaybeBidiSpaces) {
  {
    String source8("foo\t\n");
    ASSERT_TRUE(source8.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source8);
    EXPECT_EQ("foo  ", normalized);
    EXPECT_FALSE(normalized.Is8Bit());
    EXPECT_FALSE(is_bidi);
  }
  {
    String source16(u"foo\t\n");
    ASSERT_FALSE(source16.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source16);
    EXPECT_EQ("foo  ", normalized);
    EXPECT_FALSE(normalized.Is8Bit());
    EXPECT_FALSE(is_bidi);
  }
}

TEST_F(PlainTextNodeTest, NormalizeSpacesAndMaybeBidiZws) {
  {
    String source8("foo\f\r");
    ASSERT_TRUE(source8.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source8);
    EXPECT_EQ(u"foo\u200B\u200B", normalized);
    EXPECT_FALSE(normalized.Is8Bit());
    EXPECT_FALSE(is_bidi);
  }
  {
    String source16(u"foo\uFFFC\u200E");
    ASSERT_FALSE(source16.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source16);
    EXPECT_EQ(u"foo\u200B\u200B", normalized);
    EXPECT_FALSE(normalized.Is8Bit());
    EXPECT_TRUE(is_bidi);
  }
}

TEST_F(PlainTextNodeTest, NormalizeSpacesAndMaybeBidiSurrogates) {
  {
    // U+20BB7 is not in the BMP, but in CJK Unified Ideographs Extension B.
    String source16(u"\U00020BB7");
    ASSERT_FALSE(source16.Is8Bit());
    auto [normalized, is_bidi] = NormalizeSpacesAndMaybeBidi(source16);
    EXPECT_EQ(u"\U00020BB7", normalized);
    EXPECT_FALSE(normalized.Is8Bit());
    EXPECT_FALSE(is_bidi);
  }
}

TEST_F(PlainTextNodeTest, NormalizeSpacesAndMaybeBidiCanvasSpaces) {
  constexpr bool kNormalizeSpace = true;
  {
    String source8("foo\f\x0B");
    ASSERT_TRUE(source8.Is8Bit());
    auto [normalized, is_bidi] =
        NormalizeSpacesAndMaybeBidi(source8, kNormalizeSpace);
    EXPECT_EQ("foo  ", normalized);
    EXPECT_FALSE(normalized.Is8Bit());
    EXPECT_FALSE(is_bidi);
  }
  {
    String source16(u"foo\x0C\r");
    ASSERT_FALSE(source16.Is8Bit());
    auto [normalized, is_bidi] =
        NormalizeSpacesAndMaybeBidi(source16, kNormalizeSpace);
    EXPECT_EQ("foo  ", normalized);
    EXPECT_FALSE(normalized.Is8Bit());
    EXPECT_FALSE(is_bidi);
  }
}

TEST_F(PlainTextNodeTest, SegmentTextBasic) {
  TextRun run("hello world");
  PlainTextNode& node =
      CreatePlainTextNode(run, !kNormalizeSpace, !kSupportsBidi);

  ASSERT_EQ(node.ItemList().size(), 3u);

  const PlainTextItem& item = node.ItemList()[0];
  EXPECT_EQ(item.StartOffset(), 0u);
  EXPECT_EQ(item.Length(), 5u);
  EXPECT_EQ(item.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item.Text(), "hello");

  const PlainTextItem& item2 = node.ItemList()[1];
  EXPECT_EQ(item2.StartOffset(), 5u);
  EXPECT_EQ(item2.Length(), 1u);
  EXPECT_EQ(item2.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item2.Text(), " ");

  const PlainTextItem& item3 = node.ItemList()[2];
  EXPECT_EQ(item3.StartOffset(), 6u);
  EXPECT_EQ(item3.Length(), 5u);
  EXPECT_EQ(item3.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item3.Text(), "world");
}

TEST_F(PlainTextNodeTest, SegmentTextNormalizeSpaces) {
  TextRun run("hello\t world\n");
  PlainTextNode& node =
      CreatePlainTextNode(run, kNormalizeSpace, !kSupportsBidi);

  ASSERT_EQ(node.ItemList().size(), 5u);

  const PlainTextItem& item = node.ItemList()[0];
  EXPECT_EQ(item.StartOffset(), 0u);
  EXPECT_EQ(item.Length(), 5u);
  EXPECT_EQ(item.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item.Text(), "hello");

  EXPECT_EQ(node.ItemList()[1].Text(), " ");
  EXPECT_EQ(node.ItemList()[2].Text(), " ");
  EXPECT_EQ(node.ItemList()[3].Text(), "world");
  EXPECT_EQ(node.ItemList()[4].Text(), " ");
}

TEST_F(PlainTextNodeTest, SegmentTextIdeograph) {
  String text = u"\u611F\u3058foo";
  TextRun run(text);
  PlainTextNode& node =
      CreatePlainTextNode(run, !kNormalizeSpace, kSupportsBidi);
  ASSERT_EQ(node.ItemList().size(), 3u);
  EXPECT_EQ(node.ItemList()[0].Text(), u"\u611F");
  EXPECT_EQ(node.ItemList()[1].Text(), u"\u3058");
  EXPECT_EQ(node.ItemList()[2].Text(), "foo");
}

TEST_F(PlainTextNodeTest, SegmentTextBidi) {
  String text = u"123\u05E9\u05DC\u05D5\u05DD456";  // Hebrew characters
  TextRun run(text, TextDirection::kRtl);
  PlainTextNode& node =
      CreatePlainTextNode(run, !kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(node.ItemList().size(), 3u);

  const PlainTextItem& item1 = node.ItemList()[0];
  EXPECT_EQ(item1.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item1.Text(), "456");

  const PlainTextItem& item2 = node.ItemList()[1];
  EXPECT_EQ(item2.Direction(), TextDirection::kRtl);
  EXPECT_EQ(item2.Text(), u"\u05E9\u05DC\u05D5\u05DD");

  const PlainTextItem& item3 = node.ItemList()[2];
  EXPECT_EQ(item3.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item3.Text(), "123");
}

TEST_F(PlainTextNodeTest, SegmentTextBidiNoSupport) {
  String text = u"123\u05E9\u05DC\u05D5\u05DD456";  // Hebrew characters
  TextRun run(text, TextDirection::kRtl);
  PlainTextNode& node =
      CreatePlainTextNode(run, !kNormalizeSpace, !kSupportsBidi);

  ASSERT_EQ(node.ItemList().size(), 1u);
  const PlainTextItem& item1 = node.ItemList()[0];
  EXPECT_EQ(item1.Direction(), TextDirection::kRtl);
  EXPECT_EQ(item1.Text(), u"123\u05E9\u05DC\u05D5\u05DD456");
}

TEST_F(PlainTextNodeTest, SegmentTextBidiOverride) {
  String text = u"123\u05E9\u05DC\u05D5\u05DD456";  // Hebrew characters
  TextRun run(text, TextDirection::kLtr, kDirectionalOverride);
  PlainTextNode& node =
      CreatePlainTextNode(run, !kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(node.ItemList().size(), 1u);

  const PlainTextItem& item1 = node.ItemList()[0];
  EXPECT_EQ(item1.StartOffset(), 0u);
  EXPECT_EQ(item1.Length(), 10u);
  EXPECT_EQ(item1.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item1.Text(), text);
}

TEST_F(PlainTextNodeTest, SegmentTextBidiOverrideNested) {
  String text =
      u"123\u202E\u05E9\u05DC\u05D5\u05DD\u202C456";  // Hebrew characters
  TextRun run(text, TextDirection::kLtr, kDirectionalOverride);
  PlainTextNode& node =
      CreatePlainTextNode(run, !kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(node.ItemList().size(), 5u);

  const PlainTextItem& item1 = node.ItemList()[0];
  EXPECT_EQ(item1.StartOffset(), 0u);
  EXPECT_EQ(item1.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item1.Text(), "123");

  const PlainTextItem& item2 = node.ItemList()[1];
  EXPECT_EQ(item2.StartOffset(), 4u);
  EXPECT_EQ(item2.Direction(), TextDirection::kRtl);
  EXPECT_EQ(item2.Text(), u"\u05E9\u05DC\u05D5\u05DD");

  const PlainTextItem& item3 = node.ItemList()[2];
  EXPECT_EQ(item3.StartOffset(), 3u);
  EXPECT_EQ(item3.Direction(), TextDirection::kRtl);
  EXPECT_EQ(item3.Text(), u"\u200B");

  const PlainTextItem& item4 = node.ItemList()[3];
  EXPECT_EQ(item4.StartOffset(), 8u);
  EXPECT_EQ(item4.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item4.Text(), u"\u200B");

  const PlainTextItem& item5 = node.ItemList()[4];
  EXPECT_EQ(item5.StartOffset(), 9u);
  EXPECT_EQ(item5.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item5.Text(), u"456");
}

TEST_F(PlainTextNodeTest, Shape) {
  TextRun run("hello world");
  PlainTextNode& node =
      CreatePlainTextNode(run, !kNormalizeSpace, kSupportsBidi);
  for (const PlainTextItem& item : node.ItemList()) {
    EXPECT_NE(item.GetShapeResult(), nullptr);
  }
}

}  // namespace blink
