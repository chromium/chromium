// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/case_map.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/text_offset_map.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using testing::ElementsAreArray;

namespace blink {

namespace {

String To8BitOrNull(const String& source) {
  if (source.IsNull() || source.Is8Bit())
    return source;
  if (!source.ContainsOnlyLatin1OrEmpty())
    return String();
  return String::Make8BitFrom16BitSource(source.Span16());
}

}  // namespace

static struct CaseMapTestData {
  const char16_t* source;
  const char* locale;
  const char16_t* lower_expected;
  const char16_t* upper_expected;
  const char16_t* title_expected;
  std::vector<TextOffsetMap::Entry> lower_map = {};
  std::vector<TextOffsetMap::Entry> upper_map = {};
  std::vector<TextOffsetMap::Entry> title_map = {};
} case_map_test_data[] = {
    // Empty string.
    {nullptr, "", nullptr, nullptr, nullptr},
    {u"", "", u"", u"", u""},
    // Non-letters
    {u"123", "", u"123", u"123", u"123"},
    // ASCII lower/uppercases.
    {u"xyz", "", u"xyz", u"XYZ", u"Xyz"},
    {u"XYZ", "", u"xyz", u"XYZ", u"XYZ"},
    {u"Xyz", "", u"xyz", u"XYZ", u"Xyz"},
    {u"xYz", "", u"xyz", u"XYZ", u"XYz"},
    // No break space character.
    {u"abc\u00A0def", "", u"abc\u00A0def", u"ABC\u00A0DEF", u"Abc\u00A0Def"},
    // German eszett. Uppercasing and Titlecasing make the string longer.
    {u"\u00DF", "", u"\u00DF", u"SS", u"Ss", {}, {{1, 2}}, {{1, 2}}},
    {u"\u00DFz", "", u"\u00DFz", u"SSZ", u"Ssz", {}, {{1, 2}}, {{1, 2}}},
    {u"x\u00DF", "", u"x\u00DF", u"XSS", u"X\u00DF", {}, {{2, 3}}},
    {u"x\u00DFz", "", u"x\u00DFz", u"XSSZ", u"X\u00DFz", {}, {{2, 3}}},
    // Turkish/Azeri.
    {u"\u0130", "tr", u"\u0069", u"\u0130", u"\u0130"},
    // Turkish/Azeri. Lowercasing can make the string shorter.
    {u"I\u0307", "tr", u"i", u"I\u0307", u"I\u0307", {{2, 1}}},
    // Lithuanian. Uppercasing and Titlecasing can make the string shorter.
    {u"i\u0307", "lt", u"i\u0307", u"I", u"I\u0307", {}, {{2, 1}}},
    {u"i\u0307z", "lt", u"i\u0307z", u"IZ", u"I\u0307z", {}, {{2, 1}}},
    {u"xi\u0307", "lt", u"xi\u0307", u"XI", u"Xi\u0307", {}, {{3, 2}}},
    {u"xi\u0307z", "lt", u"xi\u0307z", u"XIZ", u"Xi\u0307z", {}, {{3, 2}}},
    // Lithuanian. Lowercasing can make the string longer.
    {u"\u00CC", "lt", u"\u0069\u0307\u0300", u"\u00CC", u"\u00CC", {{1, 3}}},
    // Dutch.
    {u"ij", "nl", u"ij", u"IJ", u"IJ"},
    {u"ijsland", "nl", u"ijsland", u"IJSLAND", u"IJsland"},
    {u"aij", "nl", u"aij", u"AIJ", u"Aij"},
    // Mix of longer ones and shorter ones.
    {u"\u00DFi\u0307",
     "lt",
     u"\u00DFi\u0307",
     u"SSI",
     u"Ssi\u0307",
     {},
     {{1, 2}, {3, 3}},
     {{1, 2}}},
    {u"\u00DFyi\u0307z",
     "lt",
     u"\u00DFyi\u0307z",
     u"SSYIZ",
     u"Ssyi\u0307z",
     {},
     {{1, 2}, {4, 4}},
     {{1, 2}}},
    {u"i\u0307\u00DF",
     "lt",
     u"i\u0307\u00DF",
     u"ISS",
     u"I\u0307\u00DF",
     {},
     {{2, 1}, {3, 3}}},
};

std::ostream& operator<<(std::ostream& os, const CaseMapTestData& data) {
  return os << String(data.source) << " locale=" << data.locale;
}

class CaseMapTest : public testing::Test,
                    public testing::WithParamInterface<CaseMapTestData> {};

INSTANTIATE_TEST_SUITE_P(CaseMapTest,
                         CaseMapTest,
                         testing::ValuesIn(case_map_test_data));

TEST_P(CaseMapTest, ToLowerWithoutOffset) {
  const auto data = GetParam();
  CaseMap case_map(AtomicString(data.locale));
  String source(data.source);
  String lower = case_map.ToLower(source);
  EXPECT_EQ(lower, String(data.lower_expected));
}

TEST_P(CaseMapTest, ToUpperWithoutOffset) {
  const auto data = GetParam();
  CaseMap case_map(AtomicString(data.locale));
  String source(data.source);
  String upper = case_map.ToUpper(source);
  EXPECT_EQ(upper, String(data.upper_expected));
}

TEST_P(CaseMapTest, ToTitleWithoutOffset) {
  const auto data = GetParam();
  CaseMap case_map(AtomicString(data.locale));
  String source(data.source);
  String title = case_map.ToTitle(source);
  EXPECT_EQ(title, String(data.title_expected));
}

TEST_P(CaseMapTest, ToLower) {
  const auto data = GetParam();
  CaseMap case_map(AtomicString(data.locale));
  String source(data.source);
  TextOffsetMap offset_map;
  String lower = case_map.ToLower(source, &offset_map);
  EXPECT_EQ(lower, String(data.lower_expected));
  EXPECT_THAT(offset_map.Entries(), ElementsAreArray(data.lower_map));
}

TEST_P(CaseMapTest, ToUpper) {
  const auto data = GetParam();
  CaseMap case_map(AtomicString(data.locale));
  String source(data.source);
  TextOffsetMap offset_map;
  String upper = case_map.ToUpper(source, &offset_map);
  EXPECT_EQ(upper, String(data.upper_expected));
  EXPECT_THAT(offset_map.Entries(), ElementsAreArray(data.upper_map));
}

TEST_P(CaseMapTest, ToTitle) {
  const auto data = GetParam();
  CaseMap case_map(AtomicString(data.locale));
  String source(data.source);
  TextOffsetMap offset_map;
  String title = case_map.ToTitle(source, &offset_map);
  EXPECT_EQ(title, String(data.title_expected));
  EXPECT_THAT(offset_map.Entries(), ElementsAreArray(data.title_map));
}

TEST_P(CaseMapTest, ToLower8Bit) {
  const auto data = GetParam();
  String source(data.source);
  source = To8BitOrNull(source);
  if (!source)
    return;
  CaseMap case_map(AtomicString(data.locale));
  TextOffsetMap offset_map;
  String lower = case_map.ToLower(source, &offset_map);
  EXPECT_EQ(lower, String(data.lower_expected));
  EXPECT_THAT(offset_map.Entries(), ElementsAreArray(data.lower_map));
}

TEST_P(CaseMapTest, ToUpper8Bit) {
  const auto data = GetParam();
  String source(data.source);
  source = To8BitOrNull(source);
  if (!source)
    return;
  CaseMap case_map(AtomicString(data.locale));
  TextOffsetMap offset_map;
  String upper = case_map.ToUpper(source, &offset_map);
  EXPECT_EQ(upper, String(data.upper_expected));
  EXPECT_THAT(offset_map.Entries(), ElementsAreArray(data.upper_map));
}

TEST_P(CaseMapTest, ToTitle8Bit) {
  const auto data = GetParam();
  String source(data.source);
  source = To8BitOrNull(source);
  if (!source) {
    return;
  }
  CaseMap case_map(AtomicString(data.locale));
  TextOffsetMap offset_map;
  String title = case_map.ToTitle(source, &offset_map);
  EXPECT_EQ(title, String(data.title_expected));
  EXPECT_THAT(offset_map.Entries(), ElementsAreArray(data.title_map));
}

struct CaseFoldingTestData {
  const char* source_description;
  const char* source;
  base::span<const char*> locale_list;
  const char* expected;
};

// \xC4\xB0 = U+0130 (capital dotted I)
// \xC4\xB1 = U+0131 (lowercase dotless I)
const char* g_turkic_input = "isI\xC4\xB0 \xC4\xB1s\xC4\xB0I";
const char* g_greek_input =
    "\xCE\x9F\xCE\x94\xCE\x8C\xCE\xA3 \xCE\x9F\xCE\xB4\xCF\x8C\xCF\x82 "
    "\xCE\xA3\xCE\xBF \xCE\xA3\xCE\x9F o\xCE\xA3 \xCE\x9F\xCE\xA3 \xCF\x83 "
    "\xE1\xBC\x95\xCE\xBE";
const char* g_lithuanian_input =
    "I \xC3\x8F J J\xCC\x88 \xC4\xAE \xC4\xAE\xCC\x88 \xC3\x8C \xC3\x8D "
    "\xC4\xA8 xi\xCC\x87\xCC\x88 xj\xCC\x87\xCC\x88 x\xC4\xAF\xCC\x87\xCC\x88 "
    "xi\xCC\x87\xCC\x80 xi\xCC\x87\xCC\x81 xi\xCC\x87\xCC\x83 XI X\xC3\x8F XJ "
    "XJ\xCC\x88 X\xC4\xAE X\xC4\xAE\xCC\x88";

const char* g_turkic_locales[] = {
    "tr", "tr-TR", "tr_TR", "tr@foo=bar", "tr-US", "TR", "tr-tr", "tR",
    "az", "az-AZ", "az_AZ", "az@foo=bar", "az-US", "Az", "AZ-AZ",
};
const char* g_non_turkic_locales[] = {
    "en", "en-US", "en_US", "en@foo=bar", "EN", "En",
    "ja", "el",    "fil",   "fi",         "lt",
};
const char* g_greek_locales[] = {
    "el", "el-GR", "el_GR", "el@foo=bar", "el-US", "EL", "el-gr", "eL",
};
const char* g_non_greek_locales[] = {
    "en", "en-US", "en_US", "en@foo=bar", "EN", "En",
    "ja", "tr",    "az",    "fil",        "fi", "lt",
};
const char* g_lithuanian_locales[] = {
    "lt", "lt-LT", "lt_LT", "lt@foo=bar", "lt-US", "LT", "lt-lt", "lT",
};
// Should not have "tr" or "az" because "lt" and 'tr/az' rules conflict with
// each other.
const char* g_non_lithuanian_locales[] = {
    "en", "en-US", "en_US", "en@foo=bar", "EN", "En", "ja", "fil", "fi", "el",
};

TEST(CaseMapTest, ToUpperLocale) {
  const auto test_data_list = std::to_array<CaseFoldingTestData>({
      {
          "Turkic input",
          g_turkic_input,
          g_turkic_locales,
          "\xC4\xB0SI\xC4\xB0 IS\xC4\xB0I",
      },
      {
          "Turkic input",
          g_turkic_input,
          g_non_turkic_locales,
          "ISI\xC4\xB0 IS\xC4\xB0I",
      },
      {
          "Greek input",
          g_greek_input,
          g_greek_locales,
          "\xCE\x9F\xCE\x94\xCE\x9F\xCE\xA3 \xCE\x9F\xCE\x94\xCE\x9F\xCE\xA3 "
          "\xCE\xA3\xCE\x9F \xCE\xA3\xCE\x9F \x4F\xCE\xA3 \xCE\x9F\xCE\xA3 "
          "\xCE\xA3 \xCE\x95\xCE\x9E",
      },
      {
          "Greek input",
          g_greek_input,
          g_non_greek_locales,
          "\xCE\x9F\xCE\x94\xCE\x8C\xCE\xA3 \xCE\x9F\xCE\x94\xCE\x8C\xCE\xA3 "
          "\xCE\xA3\xCE\x9F \xCE\xA3\xCE\x9F \x4F\xCE\xA3 \xCE\x9F\xCE\xA3 "
          "\xCE\xA3 \xE1\xBC\x9D\xCE\x9E",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_lithuanian_locales,
          "I \xC3\x8F J J\xCC\x88 \xC4\xAE \xC4\xAE\xCC\x88 \xC3\x8C \xC3\x8D "
          "\xC4\xA8 XI\xCC\x88 XJ\xCC\x88 X\xC4\xAE\xCC\x88 XI\xCC\x80 "
          "XI\xCC\x81 XI\xCC\x83 XI X\xC3\x8F XJ XJ\xCC\x88 X\xC4\xAE "
          "X\xC4\xAE\xCC\x88",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_non_lithuanian_locales,
          "I \xC3\x8F J J\xCC\x88 \xC4\xAE \xC4\xAE\xCC\x88 \xC3\x8C \xC3\x8D "
          "\xC4\xA8 XI\xCC\x87\xCC\x88 XJ\xCC\x87\xCC\x88 "
          "X\xC4\xAE\xCC\x87\xCC\x88 XI\xCC\x87\xCC\x80 XI\xCC\x87\xCC\x81 "
          "XI\xCC\x87\xCC\x83 XI X\xC3\x8F XJ XJ\xCC\x88 X\xC4\xAE "
          "X\xC4\xAE\xCC\x88",
      },
  });

  for (const auto& test_data : test_data_list) {
    const char* expected = test_data.expected;
    String source = String::FromUTF8(test_data.source);
    for (const auto& locale : test_data.locale_list) {
      CaseMap case_map{AtomicString(locale)};
      EXPECT_EQ(expected, case_map.ToUpper(source).Utf8())
          << test_data.source_description << "; locale=" << locale;
    }
  }
}

TEST(CaseMapTest, ToLowerLocale) {
  const auto test_data_list = std::to_array<CaseFoldingTestData>({
      {
          "Turkic input",
          g_turkic_input,
          g_turkic_locales,
          "is\xC4\xB1i \xC4\xB1si\xC4\xB1",
      },
      {
          "Turkic input",
          g_turkic_input,
          g_non_turkic_locales,
          // U+0130 is lowercased to U+0069 followed by U+0307
          "isii\xCC\x87 \xC4\xB1si\xCC\x87i",
      },
      {
          "Greek input",
          g_greek_input,
          g_greek_locales,
          "\xCE\xBF\xCE\xB4\xCF\x8C\xCF\x82 \xCE\xBF\xCE\xB4\xCF\x8C\xCF\x82 "
          "\xCF\x83\xCE\xBF \xCF\x83\xCE\xBF \x6F\xCF\x82 \xCE\xBF\xCF\x82 "
          "\xCF\x83 \xE1\xBC\x95\xCE\xBE",
      },
      {
          "Greek input",
          g_greek_input,
          g_non_greek_locales,
          "\xCE\xBF\xCE\xB4\xCF\x8C\xCF\x82 \xCE\xBF\xCE\xB4\xCF\x8C\xCF\x82 "
          "\xCF\x83\xCE\xBF \xCF\x83\xCE\xBF \x6F\xCF\x82 \xCE\xBF\xCF\x82 "
          "\xCF\x83 \xE1\xBC\x95\xCE\xBE",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_lithuanian_locales,
          "i \xC3\xAF j j\xCC\x87\xCC\x88 \xC4\xAF \xC4\xAF\xCC\x87\xCC\x88 "
          "i\xCC\x87\xCC\x80 i\xCC\x87\xCC\x81 i\xCC\x87\xCC\x83 "
          "xi\xCC\x87\xCC\x88 xj\xCC\x87\xCC\x88 x\xC4\xAF\xCC\x87\xCC\x88 "
          "xi\xCC\x87\xCC\x80 xi\xCC\x87\xCC\x81 xi\xCC\x87\xCC\x83 xi "
          "x\xC3\xAF xj xj\xCC\x87\xCC\x88 x\xC4\xAF x\xC4\xAF\xCC\x87\xCC\x88",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_non_lithuanian_locales,
          "\x69 \xC3\xAF \x6A \x6A\xCC\x88 \xC4\xAF \xC4\xAF\xCC\x88 \xC3\xAC "
          "\xC3\xAD \xC4\xA9 \x78\x69\xCC\x87\xCC\x88 \x78\x6A\xCC\x87\xCC\x88 "
          "\x78\xC4\xAF\xCC\x87\xCC\x88 \x78\x69\xCC\x87\xCC\x80 "
          "\x78\x69\xCC\x87\xCC\x81 \x78\x69\xCC\x87\xCC\x83 \x78\x69 "
          "\x78\xC3\xAF \x78\x6A \x78\x6A\xCC\x88 \x78\xC4\xAF "
          "\x78\xC4\xAF\xCC\x88",
      },
  });

  for (const auto& test_data : test_data_list) {
    const char* expected = test_data.expected;
    String source = String::FromUTF8(test_data.source);
    for (const auto& locale : test_data.locale_list) {
      CaseMap case_map{AtomicString(locale)};
      EXPECT_EQ(expected, case_map.ToLower(source).Utf8())
          << test_data.source_description << "; locale=" << locale;
    }
  }
}

TEST(CaseMapTest, ToTitleLocale) {
  const auto test_data_list = std::to_array<CaseFoldingTestData>({
      {
          "Turkic input",
          g_turkic_input,
          g_turkic_locales,
          "\xC4\xB0sI\xC4\xB0 Is\xC4\xB0I",
      },
      {
          "Turkic input",
          g_turkic_input,
          g_non_turkic_locales,
          "IsI\xC4\xB0 Is\xC4\xB0I",
      },
      {
          "Greek input",
          g_greek_input,
          g_greek_locales,
          "\xCE\x9F\xCE\x94\xCE\x8C\xCE\xA3 \xCE\x9F\xCE\xB4\xCF\x8C\xCF\x82 "
          "\xCE\xA3\xCE\xBF \xCE\xA3\xCE\x9F O\xCE\xA3 \xCE\x9F\xCE\xA3 "
          "\xCE\xA3 \xE1\xBC\x9D\xCE\xBE",
      },
      {
          "Greek input",
          g_greek_input,
          g_non_greek_locales,
          "\xCE\x9F\xCE\x94\xCE\x8C\xCE\xA3 \xCE\x9F\xCE\xB4\xCF\x8C\xCF\x82 "
          "\xCE\xA3\xCE\xBF \xCE\xA3\xCE\x9F O\xCE\xA3 \xCE\x9F\xCE\xA3 "
          "\xCE\xA3 \xE1\xBC\x9D\xCE\xBE",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_lithuanian_locales,
          "I \xC3\x8F J J\xCC\x88 \xC4\xAE \xC4\xAE\xCC\x88 \xC3\x8C \xC3\x8D "
          "\xC4\xA8 Xi\xCC\x87\xCC\x88 Xj\xCC\x87\xCC\x88 "
          "X\xC4\xAF\xCC\x87\xCC\x88 Xi\xCC\x87\xCC\x80 Xi\xCC\x87\xCC\x81 "
          "Xi\xCC\x87\xCC\x83 XI X\xC3\x8F XJ XJ\xCC\x88 X\xC4\xAE "
          "X\xC4\xAE\xCC\x88",
      },
      {
          "Lithuanian input",
          g_lithuanian_input,
          g_non_lithuanian_locales,
          "I \xC3\x8F J J\xCC\x88 \xC4\xAE \xC4\xAE\xCC\x88 \xC3\x8C \xC3\x8D "
          "\xC4\xA8 Xi\xCC\x87\xCC\x88 Xj\xCC\x87\xCC\x88 "
          "X\xC4\xAF\xCC\x87\xCC\x88 Xi\xCC\x87\xCC\x80 Xi\xCC\x87\xCC\x81 "
          "Xi\xCC\x87\xCC\x83 XI X\xC3\x8F XJ XJ\xCC\x88 X\xC4\xAE "
          "X\xC4\xAE\xCC\x88",
      },
  });

  for (const auto& test_data : test_data_list) {
    const char* expected = test_data.expected;
    String source = String::FromUTF8(test_data.source);
    for (const auto& locale : test_data.locale_list) {
      CaseMap case_map{AtomicString(locale)};
      EXPECT_EQ(expected, case_map.ToTitle(source).Utf8())
          << test_data.source_description << "; locale=" << locale;
    }
  }
}

TEST(CaseMapTest, ToTitleWithPreviousCharacter) {
  CaseMap en_case_map(AtomicString("en"));

  String input1 = String::FromUTF8("bc def");
  String result1 = en_case_map.ToTitle(input1, nullptr, u'a');
  EXPECT_EQ(result1, String::FromUTF8("bc Def"));

  String input2 = String::FromUTF8("bc");
  String result2 = en_case_map.ToTitle(input2, nullptr, u'a');
  EXPECT_EQ(result2, String::FromUTF8("bc"));

  String input3 = String::FromUTF8("abc");
  String result3 = en_case_map.ToTitle(input3, nullptr, u'@');
  EXPECT_EQ(result3, String::FromUTF8("Abc"));

  CaseMap de_case_map(AtomicString("de"));

  // \xC3\x9F = U+00DF (Latin Small Letter Sharp S)
  String input4 = String::FromUTF8("\xC3\x9F");
  String result4 = de_case_map.ToTitle(input4, nullptr, u'a');
  EXPECT_EQ(result4, String::FromUTF8("\xC3\x9F"));

  String input5 = String::FromUTF8("abc");
  String result5 = de_case_map.ToTitle(input5, nullptr, u'\u00DF');
  EXPECT_EQ(result5, String::FromUTF8("abc"));

  CaseMap nl_case_map(AtomicString("nl"));

  String input6 = String::FromUTF8("ja");
  String result6 = nl_case_map.ToTitle(input6, nullptr, u'i');
  EXPECT_EQ(result6, String::FromUTF8("ja"));
}

}  // namespace blink
