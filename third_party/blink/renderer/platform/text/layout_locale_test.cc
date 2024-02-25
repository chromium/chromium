// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/layout_locale.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(LayoutLocaleTest, Get) {
  LayoutLocale::ClearForTesting();

  EXPECT_EQ(nullptr, LayoutLocale::Get(g_null_atom));

  EXPECT_EQ(g_empty_atom, LayoutLocale::Get(g_empty_atom)->LocaleString());

  EXPECT_STRCASEEQ(
      "en-us",
      LayoutLocale::Get(AtomicString("en-us"))->LocaleString().Ascii().c_str());
  EXPECT_STRCASEEQ(
      "ja-jp",
      LayoutLocale::Get(AtomicString("ja-jp"))->LocaleString().Ascii().c_str());

  LayoutLocale::ClearForTesting();
}

TEST(LayoutLocaleTest, GetCaseInsensitive) {
  const LayoutLocale* en_us = LayoutLocale::Get(AtomicString("en-us"));
  EXPECT_EQ(en_us, LayoutLocale::Get(AtomicString("en-US")));
}

// Test combinations of BCP 47 locales.
// https://tools.ietf.org/html/bcp47
struct LocaleTestData {
  const char* locale;
  UScriptCode script;
  const char* sk_font_mgr = nullptr;
  std::optional<UScriptCode> script_for_han;
} locale_test_data[] = {
    // Country is not relevant to |SkFontMgr|.
    {"en-US", USCRIPT_LATIN, "en"},

    // Strip countries but keep scripts.
    {"en-Latn-US", USCRIPT_LATIN, "en-Latn"},

    // Common lang-script.
    {"en-Latn", USCRIPT_LATIN, "en-Latn"},
    {"ar-Arab", USCRIPT_ARABIC, "ar-Arab"},

    // Examples from `fonts.xml`.
    // https://android.googlesource.com/platform/frameworks/base/+/master/data/fonts/fonts.xml
    {"und-Arab", USCRIPT_ARABIC, "und-Arab"},
    {"und-Thai", USCRIPT_THAI, "und-Thai"},

    // Common lang-region in East Asia.
#define EXPECT_JAPANESE \
  USCRIPT_KATAKANA_OR_HIRAGANA, "ja", USCRIPT_KATAKANA_OR_HIRAGANA
#define EXPECT_KOREAN USCRIPT_HANGUL, "ko", USCRIPT_HANGUL
#define EXPECT_SIMPLIFIED_CHINESE \
  USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN
#define EXPECT_TRADITIONAL_CHINESE \
  USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN
    {"ja-JP", EXPECT_JAPANESE},
    {"ko-KR", EXPECT_KOREAN},
    {"zh", EXPECT_SIMPLIFIED_CHINESE},
    {"zh-CN", EXPECT_SIMPLIFIED_CHINESE},
    {"zh-HK", EXPECT_TRADITIONAL_CHINESE},
    {"zh-MO", EXPECT_TRADITIONAL_CHINESE},
    {"zh-SG", EXPECT_SIMPLIFIED_CHINESE},
    {"zh-TW", EXPECT_TRADITIONAL_CHINESE},

    // Encompassed languages within the Chinese macrolanguage.
    // Both "lang" and "lang-extlang" should work.
    {"nan", EXPECT_TRADITIONAL_CHINESE},
    {"wuu", EXPECT_SIMPLIFIED_CHINESE},
    {"yue", EXPECT_TRADITIONAL_CHINESE},
    {"zh-nan", EXPECT_TRADITIONAL_CHINESE},
    {"zh-wuu", EXPECT_SIMPLIFIED_CHINESE},
    {"zh-yue", EXPECT_TRADITIONAL_CHINESE},

    // Specified scripts is honored.
    {"zh-Hans", EXPECT_SIMPLIFIED_CHINESE},
    {"zh-Hant", EXPECT_TRADITIONAL_CHINESE},

    // Lowercase scripts should be capitalized.
    // |SkFontMgr_Android| uses case-sensitive match, and `fonts.xml` has
    // capitalized script names.
    {"zh-hans", EXPECT_SIMPLIFIED_CHINESE},
    {"zh-hant", EXPECT_TRADITIONAL_CHINESE},

    // Script has priority over other subtags.
    {"en-Hans", EXPECT_SIMPLIFIED_CHINESE},
    {"en-Hant", EXPECT_TRADITIONAL_CHINESE},
    {"en-Hans-TW", EXPECT_SIMPLIFIED_CHINESE},
    {"en-Hant-CN", EXPECT_TRADITIONAL_CHINESE},
    {"en-TW-Hans", EXPECT_SIMPLIFIED_CHINESE},
    {"en-CN-Hant", EXPECT_TRADITIONAL_CHINESE},
    {"wuu-Hant", EXPECT_TRADITIONAL_CHINESE},
    {"yue-Hans", EXPECT_SIMPLIFIED_CHINESE},
    {"zh-wuu-Hant", EXPECT_TRADITIONAL_CHINESE},
    {"zh-yue-Hans", EXPECT_SIMPLIFIED_CHINESE},

    // Lang has priority over region.
    // icu::Locale::getDefault() returns other combinations if, for instance,
    // English Windows with the display language set to Japanese.
    {"ja", EXPECT_JAPANESE},
    {"ja-US", EXPECT_JAPANESE},
    {"ko", EXPECT_KOREAN},
    {"ko-US", EXPECT_KOREAN},
    {"wuu-TW", EXPECT_SIMPLIFIED_CHINESE},
    {"yue-CN", EXPECT_TRADITIONAL_CHINESE},
    {"zh-wuu-TW", EXPECT_SIMPLIFIED_CHINESE},
    {"zh-yue-CN", EXPECT_TRADITIONAL_CHINESE},

    // Region should not affect script, but it can influence scriptForHan.
    {"en-CN", USCRIPT_LATIN, "en"},
    {"en-HK", USCRIPT_LATIN, "en", USCRIPT_TRADITIONAL_HAN},
    {"en-MO", USCRIPT_LATIN, "en", USCRIPT_TRADITIONAL_HAN},
    {"en-SG", USCRIPT_LATIN, "en"},
    {"en-TW", USCRIPT_LATIN, "en", USCRIPT_TRADITIONAL_HAN},
    {"en-JP", USCRIPT_LATIN, "en", USCRIPT_KATAKANA_OR_HIRAGANA},
    {"en-KR", USCRIPT_LATIN, "en", USCRIPT_HANGUL},

    // Multiple regions are invalid, but it can still give hints for the font
    // selection.
    {"en-US-JP", USCRIPT_LATIN, "en", USCRIPT_KATAKANA_OR_HIRAGANA},
};
#undef EXPECT_JAPANESE
#undef EXPECT_KOREAN
#undef EXPECT_SIMPLIFIED_CHINESE
#undef EXPECT_TRADITIONAL_CHINESE

std::ostream& operator<<(std::ostream& os, const LocaleTestData& test) {
  return os << test.locale;
}
class LocaleTestDataFixture : public testing::TestWithParam<LocaleTestData> {};

INSTANTIATE_TEST_SUITE_P(LayoutLocaleTest,
                         LocaleTestDataFixture,
                         testing::ValuesIn(locale_test_data));

TEST_P(LocaleTestDataFixture, Script) {
  const auto& test = GetParam();
  scoped_refptr<LayoutLocale> locale =
      LayoutLocale::CreateForTesting(AtomicString(test.locale));
  EXPECT_EQ(test.script, locale->GetScript()) << test.locale;
  EXPECT_EQ(test.script_for_han.has_value(), locale->HasScriptForHan())
      << test.locale;
  if (test.script_for_han) {
    EXPECT_EQ(*test.script_for_han, locale->GetScriptForHan()) << test.locale;
  } else {
    EXPECT_EQ(USCRIPT_SIMPLIFIED_HAN, locale->GetScriptForHan()) << test.locale;
  }
  if (test.sk_font_mgr)
    EXPECT_STREQ(test.sk_font_mgr, locale->LocaleForSkFontMgr()) << test.locale;
}

TEST(LayoutLocaleTest, BreakKeyword) {
  struct {
    const char* expected;
    const char* locale;
    LineBreakStrictness strictness;
    bool use_phrase = false;
  } tests[] = {
      {nullptr, nullptr, LineBreakStrictness::kDefault},
      {"", "", LineBreakStrictness::kDefault},
      {nullptr, nullptr, LineBreakStrictness::kStrict},
      {"", "", LineBreakStrictness::kStrict},
      {"ja", "ja", LineBreakStrictness::kDefault},
      {"ja@lb=normal", "ja", LineBreakStrictness::kNormal},
      {"ja@lb=strict", "ja", LineBreakStrictness::kStrict},
      {"ja@lb=loose", "ja", LineBreakStrictness::kLoose},
      {"ja@lw=phrase", "ja", LineBreakStrictness::kDefault, true},
      {"ja@lb=normal;lw=phrase", "ja", LineBreakStrictness::kNormal, true},
      {"ja@lb=strict;lw=phrase", "ja", LineBreakStrictness::kStrict, true},
      {"ja@lb=loose;lw=phrase", "ja", LineBreakStrictness::kLoose, true},
  };
  for (const auto& test : tests) {
    scoped_refptr<LayoutLocale> locale =
        LayoutLocale::CreateForTesting(AtomicString(test.locale));
    EXPECT_EQ(test.expected,
              locale->LocaleWithBreakKeyword(test.strictness, test.use_phrase))
        << String::Format("'%s' with line-break %d, phrase=%d should be '%s'",
                          test.locale, static_cast<int>(test.strictness),
                          static_cast<int>(test.use_phrase), test.expected);
  }
}

TEST(LayoutLocaleTest, GetQuotesData) {
  auto enQuotes = (QuotesData::Create(0x201c, 0x201d, 0x2018, 0x2019));
  auto frQuotes = (QuotesData::Create(0xab, 0xbb, 0xab, 0xbb));
  auto frCAQuotes = (QuotesData::Create(0xab, 0xbb, 0x201d, 0x201c));
  struct {
    const char* locale;
    const scoped_refptr<QuotesData> expected;
  } tests[] = {
      {nullptr, nullptr},    // no match
      {"loc-DNE", nullptr},  // no match
      {"en", enQuotes},      {"fr", frQuotes},
      {"fr-CA", frCAQuotes}, {"fr-DNE", frQuotes},  // use fr
  };
  for (const auto& test : tests) {
    scoped_refptr<LayoutLocale> locale =
        LayoutLocale::CreateForTesting(AtomicString(test.locale));
    scoped_refptr<QuotesData> quotes = locale->GetQuotesData();
    if (test.expected) {
      EXPECT_EQ(test.expected->GetOpenQuote(0), quotes->GetOpenQuote(0));
      EXPECT_EQ(test.expected->GetOpenQuote(1), quotes->GetOpenQuote(1));
      EXPECT_EQ(test.expected->GetCloseQuote(-1), quotes->GetCloseQuote(-1));
      EXPECT_EQ(test.expected->GetCloseQuote(0), quotes->GetCloseQuote(0));
    } else {
      EXPECT_EQ(test.expected, quotes);
    }
  }
}

TEST(LayoutLocaleTest, ExistingKeywordName) {
  const char* tests[] = {
      "en@x=", "en@lb=xyz", "en@ =",
  };
  for (auto* const test : tests) {
    scoped_refptr<LayoutLocale> locale =
        LayoutLocale::CreateForTesting(AtomicString(test));
    EXPECT_EQ(test,
              locale->LocaleWithBreakKeyword(LineBreakStrictness::kNormal));
  }
}

TEST(LayoutLocaleTest, AcceptLanguagesChanged) {
  struct {
    const char* accept_languages;
    UScriptCode script;
    const char* locale;
  } tests[] = {
      // Non-Han script cases.
      {nullptr, USCRIPT_COMMON, nullptr},
      {"", USCRIPT_COMMON, nullptr},
      {"en-US", USCRIPT_COMMON, nullptr},
      {",en-US", USCRIPT_COMMON, nullptr},

      // Single value cases.
      {"ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja"},
      {"ko-KR", USCRIPT_HANGUL, "ko"},
      {"zh-CN", USCRIPT_SIMPLIFIED_HAN, "zh-Hans"},
      {"zh-HK", USCRIPT_TRADITIONAL_HAN, "zh-Hant"},
      {"zh-TW", USCRIPT_TRADITIONAL_HAN, "zh-Hant"},

      // Language only.
      {"ja", USCRIPT_KATAKANA_OR_HIRAGANA, "ja"},
      {"ko", USCRIPT_HANGUL, "ko"},
      {"zh", USCRIPT_SIMPLIFIED_HAN, "zh-Hans"},

      // Unusual combinations.
      {"en-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja"},

      // Han scripts not in the first item.
      {"en-US,ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja"},
      {"en-US,en-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja"},

      // Multiple Han scripts. The first one wins.
      {"ja-JP,zh-CN", USCRIPT_KATAKANA_OR_HIRAGANA, "ja"},
      {"zh-TW,ja-JP", USCRIPT_TRADITIONAL_HAN, "zh-Hant"},
  };

  for (const auto& test : tests) {
    LayoutLocale::AcceptLanguagesChanged(test.accept_languages);
    const LayoutLocale* locale = LayoutLocale::LocaleForHan(nullptr);

    if (test.script == USCRIPT_COMMON) {
      EXPECT_EQ(nullptr, locale) << test.accept_languages;
      continue;
    }

    ASSERT_NE(nullptr, locale) << test.accept_languages;
    EXPECT_EQ(test.script, locale->GetScriptForHan()) << test.accept_languages;
    EXPECT_STREQ(test.locale, locale->LocaleForHanForSkFontMgr())
        << test.accept_languages;
  }
}

}  // namespace blink
