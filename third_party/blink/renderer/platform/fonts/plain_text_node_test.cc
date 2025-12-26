// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/fonts/plain_text_node.h"

#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/fonts/font.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/shaping/shape_result_test_info.h"
#include "third_party/blink/renderer/platform/text/text_run.h"

namespace blink {

namespace {

inline const ShapeResultTestInfo* TestInfo(const ShapeResult* result) {
  return static_cast<const ShapeResultTestInfo*>(result);
}

}  // namespace

class PlainTextNodeTest : public testing::Test {
 public:
  static std::pair<String, bool> NormalizeSpacesAndMaybeBidi(
      StringView text,
      bool normalize_space = false) {
    return PlainTextNode::NormalizeSpacesAndMaybeBidi(text, normalize_space);
  }

  static Font& TestFont() {
    FontDescription desc;
    desc.SetLocale(LayoutLocale::Get(AtomicString("en")));
    return *MakeGarbageCollected<Font>(desc, nullptr);
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

  void SetUp() override { skia::InitializeFontRendering(); }
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

TEST_F(PlainTextNodeTest, SegmentLatinLeftToRight) {
  TextRun text_run(base::byte_span_from_cstring("ABC DEF."));
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);
  unsigned start_index = 0;
  unsigned num_glyphs = 0;
  hb_script_t script = HB_SCRIPT_INVALID;

  ASSERT_EQ(3u, node.ItemList().size());
  const ShapeResult* result = node.ItemList()[0].GetShapeResult();
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(3u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);

  result = node.ItemList()[1].GetShapeResult();
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);

  result = node.ItemList()[2].GetShapeResult();
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, start_index);
  EXPECT_EQ(4u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_LATIN, script);
}

TEST_F(PlainTextNodeTest, SegmentCommonAccentLeftToRight) {
  const UChar kStr[] = {0x2F, 0x301, 0x2E, 0x20, 0x2E};
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);
  unsigned start_index = 0;
  unsigned num_glyphs = 0;
  hb_script_t script = HB_SCRIPT_INVALID;

  ASSERT_EQ(3u, node.ItemList().size());
  unsigned offset = 0;
  const ShapeResult* result = node.ItemList()[0].GetShapeResult();
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(0u, offset + start_index);
  EXPECT_EQ(3u, num_glyphs);
#if U_ICU_VERSION_MAJOR_NUM >= 76
  EXPECT_EQ(HB_SCRIPT_CHEROKEE, script);
#else
  EXPECT_EQ(HB_SCRIPT_COMMON, script);
#endif
  offset += result->NumCharacters();

  result = node.ItemList()[1].GetShapeResult();
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(3u, offset + start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_COMMON, script);
  offset += result->NumCharacters();

  result = node.ItemList()[2].GetShapeResult();
  ASSERT_TRUE(
      TestInfo(result)->RunInfoForTesting(0, start_index, num_glyphs, script));
  EXPECT_EQ(4u, offset + start_index);
  EXPECT_EQ(1u, num_glyphs);
  EXPECT_EQ(HB_SCRIPT_COMMON, script);
  offset += result->NumCharacters();

  ASSERT_EQ(5u, offset);
}

TEST_F(PlainTextNodeTest, SegmentCjkByCharacter) {
  const UChar kStr[] = {0x56FD, 0x56FD,  // CJK Unified Ideograph
                        'a',    'b',
                        0x56FD,  // CJK Unified Ideograph
                        'x',    'y',    'z',
                        0x3042,   // HIRAGANA LETTER A
                        0x56FD};  // CJK Unified Ideograph
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(7u, node.ItemList().size());
  const ShapeResult* word_result = node.ItemList()[0].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());
  word_result = node.ItemList()[1].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());

  word_result = node.ItemList()[2].GetShapeResult();
  EXPECT_EQ(2u, word_result->NumCharacters());

  word_result = node.ItemList()[3].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());

  word_result = node.ItemList()[4].GetShapeResult();
  EXPECT_EQ(3u, word_result->NumCharacters());

  word_result = node.ItemList()[5].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());
  word_result = node.ItemList()[6].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());
}

TEST_F(PlainTextNodeTest, SegmentCjkAndCommon) {
  const UChar kStr[] = {'a',    'b',
                        0xFF08,   // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0x56FD,   // CJK Unified Ideograph
                        0x56FD,   // CJK Unified Ideograph
                        0x56FD,   // CJK Unified Ideograph
                        0x3002};  // IDEOGRAPHIC FULL STOP (script=common)
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(4u, node.ItemList().size());
  const ShapeResult* word_result = node.ItemList()[0].GetShapeResult();
  EXPECT_EQ(2u, word_result->NumCharacters());

  word_result = node.ItemList()[1].GetShapeResult();
  EXPECT_EQ(2u, word_result->NumCharacters());

  word_result = node.ItemList()[2].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());

  word_result = node.ItemList()[3].GetShapeResult();
  EXPECT_EQ(2u, word_result->NumCharacters());
}

TEST_F(PlainTextNodeTest, SegmentCjkAndInherit) {
  const UChar kStr[] = {
      0x304B,   // HIRAGANA LETTER KA
      0x304B,   // HIRAGANA LETTER KA
      0x3009,   // COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
      0x304B};  // HIRAGANA LETTER KA
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(3u, node.ItemList().size());
  const ShapeResult* word_result = node.ItemList()[0].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());

  word_result = node.ItemList()[1].GetShapeResult();
  EXPECT_EQ(2u, word_result->NumCharacters());

  word_result = node.ItemList()[2].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());
}

TEST_F(PlainTextNodeTest, SegmentCjkAndNonCjkCommon) {
  const UChar kStr[] = {0x56FD,  // CJK Unified Ideograph
                        ' '};
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(2u, node.ItemList().size());
  const ShapeResult* word_result = node.ItemList()[0].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());

  word_result = node.ItemList()[1].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());
}

TEST_F(PlainTextNodeTest, SegmentEmojiSequences) {
  std::vector<std::string> test_strings = {
      // A family followed by a couple with heart emoji sequence,
      // the latter including a variation selector.
      "\U0001f468\u200D\U0001f469\u200D\U0001f467\u200D\U0001f466\U0001f469"
      "\u200D\u2764\uFE0F\u200D\U0001f48b\u200D\U0001f468",
      // Pirate flag
      "\U0001F3F4\u200D\u2620\uFE0F",
      // Pilot, judge sequence
      "\U0001f468\U0001f3fb\u200D\u2696\uFE0F\U0001f468\U0001f3fb\u200D\u2708"
      "\uFE0F",
      // Woman, Kiss, Man sequence
      "\U0001f469\u200D\u2764\uFE0F\u200D\U0001f48b\u200D\U0001f468",
      // Signs of horns with skin tone modifier
      "\U0001f918\U0001f3fb",
      // Man, dark skin tone, red hair
      "\U0001f468\U0001f3ff\u200D\U0001f9b0"};

  for (auto test_string : test_strings) {
    String emoji_string = String::FromUTF8(test_string);
    TextRun text_run(emoji_string);
    PlainTextNode& node =
        CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

    ASSERT_EQ(1u, node.ItemList().size());
    EXPECT_EQ(emoji_string.length(),
              node.ItemList()[0].GetShapeResult()->NumCharacters())
        << " Length mismatch for sequence: " << test_string;
  }
}

TEST_F(PlainTextNodeTest, SegmentEmojiExtraZwjPrefix) {
  // A ZWJ, followed by a family and a heart-kiss sequence.
  const UChar kStr[] = {0x200D, 0xD83D, 0xDC68, 0x200D, 0xD83D, 0xDC69,
                        0x200D, 0xD83D, 0xDC67, 0x200D, 0xD83D, 0xDC66,
                        0xD83D, 0xDC69, 0x200D, 0x2764, 0xFE0F, 0x200D,
                        0xD83D, 0xDC8B, 0x200D, 0xD83D, 0xDC68};
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(2u, node.ItemList().size());
  const ShapeResult* word_result = node.ItemList()[0].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());

  word_result = node.ItemList()[1].GetShapeResult();
  EXPECT_EQ(22u, word_result->NumCharacters());
}

TEST_F(PlainTextNodeTest, SegmentEmojiSubdivisionFlags) {
  // Subdivision flags for Wales, Scotland, England.
  const UChar kStr[] = {0xD83C, 0xDFF4, 0xDB40, 0xDC67, 0xDB40, 0xDC62, 0xDB40,
                        0xDC77, 0xDB40, 0xDC6C, 0xDB40, 0xDC73, 0xDB40, 0xDC7F,
                        0xD83C, 0xDFF4, 0xDB40, 0xDC67, 0xDB40, 0xDC62, 0xDB40,
                        0xDC73, 0xDB40, 0xDC63, 0xDB40, 0xDC74, 0xDB40, 0xDC7F,
                        0xD83C, 0xDFF4, 0xDB40, 0xDC67, 0xDB40, 0xDC62, 0xDB40,
                        0xDC65, 0xDB40, 0xDC6E, 0xDB40, 0xDC67, 0xDB40, 0xDC7F};
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(1u, node.ItemList().size());
  EXPECT_EQ(42u, node.ItemList()[0].GetShapeResult()->NumCharacters());
}

TEST_F(PlainTextNodeTest, SegmentCjkCommon) {
  const UChar kStr[] = {0xFF08,   // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0xFF08,   // FULLWIDTH LEFT PARENTHESIS (script=common)
                        0xFF08};  // FULLWIDTH LEFT PARENTHESIS (script=common)
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(1u, node.ItemList().size());
  EXPECT_EQ(3u, node.ItemList()[0].GetShapeResult()->NumCharacters());
}

TEST_F(PlainTextNodeTest, SegmentCjkCommonAndNonCjk) {
  const UChar kStr[] = {0xFF08,  // FULLWIDTH LEFT PARENTHESIS (script=common)
                        'a', 'b'};
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(2u, node.ItemList().size());
  const ShapeResult* word_result = node.ItemList()[0].GetShapeResult();
  EXPECT_EQ(1u, word_result->NumCharacters());

  word_result = node.ItemList()[1].GetShapeResult();
  EXPECT_EQ(2u, word_result->NumCharacters());
}

TEST_F(PlainTextNodeTest, SegmentCjkSmallFormVariants) {
  const UChar kStr[] = {0x5916,   // CJK UNIFIED IDEOGRPAH
                        0xFE50};  // SMALL COMMA
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(1u, node.ItemList().size());
  EXPECT_EQ(2u, node.ItemList()[0].GetShapeResult()->NumCharacters());
}

TEST_F(PlainTextNodeTest, SegmentHangulToneMark) {
  const UChar kStr[] = {0xC740,   // HANGUL SYLLABLE EUN
                        0x302E};  // HANGUL SINGLE DOT TONE MARK
  TextRun text_run{base::span(kStr)};
  PlainTextNode& node =
      CreatePlainTextNode(text_run, kNormalizeSpace, kSupportsBidi);

  ASSERT_EQ(1u, node.ItemList().size());
  EXPECT_EQ(2u, node.ItemList()[0].GetShapeResult()->NumCharacters());
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
