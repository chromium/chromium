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

  static constexpr bool kNormalizeSpace = true;
  static constexpr bool kBidiOverridden = true;
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
  PlainTextNode& node = *MakeGarbageCollected<PlainTextNode>(
      run, !kNormalizeSpace, !kBidiOverridden, TestFont(), !kSupportsBidi);

  ASSERT_EQ(node.ItemList().size(), 1u);
  const PlainTextItem& item = node.ItemList()[0];
  EXPECT_EQ(item.StartOffset(), 0u);
  EXPECT_EQ(item.Length(), 11u);
  EXPECT_EQ(item.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item.Text(), "hello world");
}

TEST_F(PlainTextNodeTest, SegmentTextNormalizeSpaces) {
  TextRun run("hello\t world\n");
  PlainTextNode& node = *MakeGarbageCollected<PlainTextNode>(
      run, kNormalizeSpace, !kBidiOverridden, TestFont(), !kSupportsBidi);

  ASSERT_EQ(node.ItemList().size(), 1u);
  const PlainTextItem& item = node.ItemList()[0];
  EXPECT_EQ(item.StartOffset(), 0u);
  EXPECT_EQ(item.Length(), 13u);
  EXPECT_EQ(item.Direction(), TextDirection::kLtr);
  EXPECT_EQ(item.Text(), "hello  world ");
}

TEST_F(PlainTextNodeTest, SegmentTextBidi) {
  String text = u"123\u05E9\u05DC\u05D5\u05DD456";  // Hebrew characters
  TextRun run(text, TextDirection::kRtl);
  PlainTextNode& node = *MakeGarbageCollected<PlainTextNode>(
      run, !kNormalizeSpace, !kBidiOverridden, TestFont(), kSupportsBidi);

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
  PlainTextNode& node = *MakeGarbageCollected<PlainTextNode>(
      run, !kNormalizeSpace, !kBidiOverridden, TestFont(), !kSupportsBidi);

  ASSERT_EQ(node.ItemList().size(), 1u);
  const PlainTextItem& item1 = node.ItemList()[0];
  EXPECT_EQ(item1.Direction(), TextDirection::kRtl);
  EXPECT_EQ(item1.Text(), u"123\u05E9\u05DC\u05D5\u05DD456");
}

}  // namespace blink
