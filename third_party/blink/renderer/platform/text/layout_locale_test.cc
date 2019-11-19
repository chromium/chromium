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

TEST(LayoutLocaleTest, ScriptTest) {
  // Test combinations of BCP 47 locales.
  // https://tools.ietf.org/html/bcp47
  struct {
    const char* locale;
    UScriptCode script;
    bool has_script_for_han;
    UScriptCode script_for_han;
  } tests[] = {
      {"en-US", USCRIPT_LATIN},

      // Common lang-script.
      {"en-Latn", USCRIPT_LATIN},
      {"ar-Arab", USCRIPT_ARABIC},

      // Common lang-region in East Asia.
      {"ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA, true},
      {"ko-KR", USCRIPT_HANGUL, true},
      {"zh", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-CN", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-HK", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-MO", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-SG", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-TW", USCRIPT_TRADITIONAL_HAN, true},

      // Encompassed languages within the Chinese macrolanguage.
      // Both "lang" and "lang-extlang" should work.
      {"nan", USCRIPT_TRADITIONAL_HAN, true},
      {"wuu", USCRIPT_SIMPLIFIED_HAN, true},
      {"yue", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-nan", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-wuu", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-yue", USCRIPT_TRADITIONAL_HAN, true},

      // Script has priority over other subtags.
      {"zh-Hant", USCRIPT_TRADITIONAL_HAN, true},
      {"en-Hans", USCRIPT_SIMPLIFIED_HAN, true},
      {"en-Hant", USCRIPT_TRADITIONAL_HAN, true},
      {"en-Hans-TW", USCRIPT_SIMPLIFIED_HAN, true},
      {"en-Hant-CN", USCRIPT_TRADITIONAL_HAN, true},
      {"wuu-Hant", USCRIPT_TRADITIONAL_HAN, true},
      {"yue-Hans", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-wuu-Hant", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-yue-Hans", USCRIPT_SIMPLIFIED_HAN, true},

      // Lang has priority over region.
      // icu::Locale::getDefault() returns other combinations if, for instnace,
      // English Windows with the display language set to Japanese.
      {"ja", USCRIPT_KATAKANA_OR_HIRAGANA, true},
      {"ja-US", USCRIPT_KATAKANA_OR_HIRAGANA, true},
      {"ko", USCRIPT_HANGUL, true},
      {"ko-US", USCRIPT_HANGUL, true},
      {"wuu-TW", USCRIPT_SIMPLIFIED_HAN, true},
      {"yue-CN", USCRIPT_TRADITIONAL_HAN, true},
      {"zh-wuu-TW", USCRIPT_SIMPLIFIED_HAN, true},
      {"zh-yue-CN", USCRIPT_TRADITIONAL_HAN, true},

      // Region should not affect script, but it can influence scriptForHan.
      {"en-CN", USCRIPT_LATIN, false},
      {"en-HK", USCRIPT_LATIN, true, USCRIPT_TRADITIONAL_HAN},
      {"en-MO", USCRIPT_LATIN, true, USCRIPT_TRADITIONAL_HAN},
      {"en-SG", USCRIPT_LATIN, false},
      {"en-TW", USCRIPT_LATIN, true, USCRIPT_TRADITIONAL_HAN},
      {"en-JP", USCRIPT_LATIN, true, USCRIPT_KATAKANA_OR_HIRAGANA},
      {"en-KR", USCRIPT_LATIN, true, USCRIPT_HANGUL},

      // Multiple regions are invalid, but it can still give hints for the font
      // selection.
      {"en-US-JP", USCRIPT_LATIN, true, USCRIPT_KATAKANA_OR_HIRAGANA},
  };

  for (const auto& test : tests) {
    scoped_refptr<LayoutLocale> locale =
        LayoutLocale::CreateForTesting(test.locale);
    EXPECT_EQ(test.script, locale->GetScript()) << test.locale;
    EXPECT_EQ(test.has_script_for_han, locale->HasScriptForHan())
        << test.locale;
    if (!test.has_script_for_han) {
      EXPECT_EQ(USCRIPT_SIMPLIFIED_HAN, locale->GetScriptForHan())
          << test.locale;
    } else if (test.script_for_han) {
      EXPECT_EQ(test.script_for_han, locale->GetScriptForHan()) << test.locale;
    } else {
      EXPECT_EQ(test.script, locale->GetScriptForHan()) << test.locale;
    }
  }
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
      {"ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},
      {"ko-KR", USCRIPT_HANGUL, "ko-kr"},
      {"zh-CN", USCRIPT_SIMPLIFIED_HAN, "zh-Hans"},
      {"zh-HK", USCRIPT_TRADITIONAL_HAN, "zh-Hant"},
      {"zh-TW", USCRIPT_TRADITIONAL_HAN, "zh-Hant"},

      // Language only.
      {"ja", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},
      {"ko", USCRIPT_HANGUL, "ko-kr"},
      {"zh", USCRIPT_SIMPLIFIED_HAN, "zh-Hans"},

      // Unusual combinations.
      {"en-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},

      // Han scripts not in the first item.
      {"en-US,ja-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},
      {"en-US,en-JP", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},

      // Multiple Han scripts. The first one wins.
      {"ja-JP,zh-CN", USCRIPT_KATAKANA_OR_HIRAGANA, "ja-jp"},
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
    EXPECT_STRCASEEQ(test.locale, locale->LocaleForHanForSkFontMgr())
        << test.accept_languages;
  }
}

}  // namespace blink
