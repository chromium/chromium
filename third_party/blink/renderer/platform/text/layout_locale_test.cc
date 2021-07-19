// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/layout_locale.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(LayoutLocaleTest, Get) {
  LayoutLocale::ClearForTesting();

  EXPECT_EQ(nullptr, LayoutLocale::Get(g_null_atom));

  EXPECT_EQ(g_empty_atom, LayoutLocale::Get(g_empty_atom)->LocaleString());

  EXPECT_STRCASEEQ("en-us",
                   LayoutLocale::Get("en-us")->LocaleString().Ascii().c_str());
  EXPECT_STRCASEEQ("ja-jp",
                   LayoutLocale::Get("ja-jp")->LocaleString().Ascii().c_str());

  LayoutLocale::ClearForTesting();
}

TEST(LayoutLocaleTest, GetCaseInsensitive) {
  const LayoutLocale* en_us = LayoutLocale::Get("en-us");
  EXPECT_EQ(en_us, LayoutLocale::Get("en-US"));
}

// Test combinations of BCP 47 locales.
// https://tools.ietf.org/html/bcp47
struct LocaleTestData {
  const char* locale;
  UScriptCode script;
  const char* sk_font_mgr = nullptr;
  absl::optional<UScriptCode> script_for_han;
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
    {"ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja", USCRIPT_KATAKANA_OR_HIRAGANA},
    {"ko-KR", USCRIPT_HANGUL, "ko", USCRIPT_HANGUL},
    {"zh", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"zh-CN", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"zh-HK", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"zh-MO", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"zh-SG", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"zh-TW", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},

    // Encompassed languages within the Chinese macrolanguage.
    // Both "lang" and "lang-extlang" should work.
    {"nan", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"wuu", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"yue", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"zh-nan", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"zh-wuu", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"zh-yue", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},

    // Specified scripts is honored.
    {"zh-Hans", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"zh-Hant", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},

    // Lowercase scripts should be capitalized.
    // |SkFontMgr_Android| uses case-sensitive match, and `fonts.xml` has
    // capitalized script names.
    {"zh-hans", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"zh-hant", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},

    // Script has priority over other subtags.
    {"en-Hans", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"en-Hant", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"en-Hans-TW", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"en-Hant-CN", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"en-TW-Hans", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"en-CN-Hant", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"wuu-Hant", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"yue-Hans", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"zh-wuu-Hant", USCRIPT_TRADITIONAL_HAN, "zh-Hant",
     USCRIPT_TRADITIONAL_HAN},
    {"zh-yue-Hans", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},

    // Lang has priority over region.
    // icu::Locale::getDefault() returns other combinations if, for instance,
    // English Windows with the display language set to Japanese.
    {"ja", USCRIPT_KATAKANA_OR_HIRAGANA, "ja", USCRIPT_KATAKANA_OR_HIRAGANA},
    {"ja-US", USCRIPT_KATAKANA_OR_HIRAGANA, "ja", USCRIPT_KATAKANA_OR_HIRAGANA},
    {"ko", USCRIPT_HANGUL, "ko", USCRIPT_HANGUL},
    {"ko-US", USCRIPT_HANGUL, "ko", USCRIPT_HANGUL},
    {"wuu-TW", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"yue-CN", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},
    {"zh-wuu-TW", USCRIPT_SIMPLIFIED_HAN, "zh-Hans", USCRIPT_SIMPLIFIED_HAN},
    {"zh-yue-CN", USCRIPT_TRADITIONAL_HAN, "zh-Hant", USCRIPT_TRADITIONAL_HAN},

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

std::ostream& operator<<(std::ostream& os, const LocaleTestData& test) {
  return os << test.locale;
}
class LocaleTestDataFixture : public testing::TestWithParam<LocaleTestData> {};

INSTANTIATE_TEST_CASE_P(LayoutLocaleTest,
                        LocaleTestDataFixture,
                        testing::ValuesIn(locale_test_data));

TEST_P(LocaleTestDataFixture, Script) {
  const auto& test = GetParam();
  scoped_refptr<LayoutLocale> locale =
      LayoutLocale::CreateForTesting(test.locale);
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
    LineBreakIteratorMode mode;
  } tests[] = {
      {nullptr, nullptr, LineBreakIteratorMode::kDefault},
      {"", "", LineBreakIteratorMode::kDefault},
      {nullptr, nullptr, LineBreakIteratorMode::kStrict},
      {"", "", LineBreakIteratorMode::kStrict},
      {"ja", "ja", LineBreakIteratorMode::kDefault},
      {"ja@lb=normal", "ja", LineBreakIteratorMode::kNormal},
      {"ja@lb=strict", "ja", LineBreakIteratorMode::kStrict},
      {"ja@lb=loose", "ja", LineBreakIteratorMode::kLoose},
  };
  for (const auto& test : tests) {
    scoped_refptr<LayoutLocale> locale =
        LayoutLocale::CreateForTesting(test.locale);
    EXPECT_EQ(test.expected, locale->LocaleWithBreakKeyword(test.mode))
        << String::Format("'%s' with line-break %d should be '%s'", test.locale,
                          static_cast<int>(test.mode), test.expected);
  }
}

TEST(LayoutLocaleTest, ExistingKeywordName) {
  const char* tests[] = {
      "en@x=", "en@lb=xyz", "en@ =",
  };
  for (auto* const test : tests) {
    scoped_refptr<LayoutLocale> locale = LayoutLocale::CreateForTesting(test);
    EXPECT_EQ(test,
              locale->LocaleWithBreakKeyword(LineBreakIteratorMode::kNormal));
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
