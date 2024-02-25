// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/model/speech_input_locale_config_impl.h"

#import "base/strings/utf_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Returns the list of VoiceSearchLanguage for the tests.
NSArray<VoiceSearchLanguage*>* GetVoiceSearchLanguagesForTests() {
  return @[
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"ar-ae"
                                        displayName:@"العربية (الإمارات)"
                             localizationPreference:@"ar-ae"],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"ar-bh"
                                        displayName:@"العربية (البحرين)"
                             localizationPreference:@"ar-bh"],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"ar-eg"
                                        displayName:@"العربية (مصر)"
                             localizationPreference:@"ar-eg"],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"cmn-hans-cn"
                                        displayName:@"普通话 (中国大陆)"
                             localizationPreference:@"zh-cn"],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"cmn-hans-hk"
                                        displayName:@"普通话 (香港)"
                             localizationPreference:@"zh-cn"],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"cmn-hant-tw"
                                        displayName:@"中文 (台灣)"
                             localizationPreference:@"zh-tw"],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"en-gb"
                                        displayName:@"English (Great Britain)"
                             localizationPreference:@"en-gb"],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"en-us"
                                        displayName:@"English (United States)"
                             localizationPreference:@"en-us"],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"fr-fr"
                                        displayName:@"Français (France)"
                             localizationPreference:@"fr-fr"],
    [[VoiceSearchLanguage alloc] initWithIdentifier:@"pl-pl"
                                        displayName:@"Polski (Polska)"
                             localizationPreference:@"pl-pl"],
  ];
}

// Returns the list of SpeechInputLocaleMatch for the tests.
NSArray<SpeechInputLocaleMatch*>* GetSpeechInputLocaleMatchesForTests() {
  return @[
    [[SpeechInputLocaleMatch alloc] initWithMatchedLocale:@"ar-AE"
                                          matchingLocales:nil
                                        matchingLanguages:@[ @"ar" ]],
    [[SpeechInputLocaleMatch alloc] initWithMatchedLocale:@"en-US"
                                          matchingLocales:@[ @"en-CA" ]
                                        matchingLanguages:@[ @"en" ]],
    [[SpeechInputLocaleMatch alloc] initWithMatchedLocale:@"zh-Hans-CN"
                                          matchingLocales:@[ @"zh-CN" ]
                                        matchingLanguages:@[ @"zh" ]],
    [[SpeechInputLocaleMatch alloc] initWithMatchedLocale:@"zh-Hans-HK"
                                          matchingLocales:@[ @"zh-HK" ]
                                        matchingLanguages:nil],
  ];
}

}  // namespace

class SpeechInputLocaleConfigImplTest : public PlatformTest {
 protected:
  SpeechInputLocaleConfigImplTest()
      : config_(GetVoiceSearchLanguagesForTests(),
                GetSpeechInputLocaleMatchesForTests()) {}

  const voice::SpeechInputLocaleConfig* config() const { return &config_; }

  // Returns the code for the SpeechInputLocale matching `locale_code`.
  std::string GetMatchingLocaleForCode(const std::string& locale_code) {
    return config_.GetMatchingLocale(locale_code).code;
  }

  voice::SpeechInputLocaleConfigImpl config_;
};

// Tests that invalid locale codes get matched with en-US.
TEST_F(SpeechInputLocaleConfigImplTest, MatchingLocaleForInvalidCode) {
  const std::string kEnglishUS("en-US");
  EXPECT_EQ(kEnglishUS, GetMatchingLocaleForCode("xx"));
  EXPECT_EQ(kEnglishUS, GetMatchingLocaleForCode(""));
}

// Checks that en-US codes are correctly matched with the en-US
// SpeechInpuLocale according to the SpeechInputLocaleMatch list.
TEST_F(SpeechInputLocaleConfigImplTest, DefaultForUS) {
  const std::string kEnglishUS("en-US");
  EXPECT_EQ(kEnglishUS, GetMatchingLocaleForCode("en-US"));
  EXPECT_EQ(kEnglishUS, GetMatchingLocaleForCode("en-US@calender=gregorian"));
}

// Tests that regional variant locale codes are correctly matched to
// the locale indicated by the SpeechInputLocaleMatch list.
TEST_F(SpeechInputLocaleConfigImplTest, RegionalLanguageVariantsMapping) {
  // French.
  const std::string kFrenchFR("fr-FR");
  EXPECT_EQ(kFrenchFR, GetMatchingLocaleForCode("fr-FR"));
  EXPECT_EQ(kFrenchFR, GetMatchingLocaleForCode("fr-BE"));
  // English UI defaults to en-US unless there is a closer match.
  const std::string kEnglishUS("en-US");
  EXPECT_EQ(kEnglishUS, GetMatchingLocaleForCode("en-US"));
  EXPECT_EQ(kEnglishUS, GetMatchingLocaleForCode("en-FR"));
  EXPECT_EQ(kEnglishUS, GetMatchingLocaleForCode("en-CA"));
  // Chinese UI defaults to zh-CN unless there is a closer match.
  const std::string kChinesCN("zh-Hans-CN");
  EXPECT_EQ(kChinesCN, GetMatchingLocaleForCode("zh-CN"));
  EXPECT_EQ(kChinesCN, GetMatchingLocaleForCode("zh-US"));
  EXPECT_EQ(std::string("zh-Hans-HK"), GetMatchingLocaleForCode("zh-HK"));
  EXPECT_EQ(std::string("zh-Hant-TW"), GetMatchingLocaleForCode("zh-TW"));
  // Arabic UI defaults to Arabic U.A.E.
  const std::string kArabicAE("ar-AE");
  EXPECT_EQ(kArabicAE, GetMatchingLocaleForCode("ar-AE"));
  EXPECT_EQ(kArabicAE, GetMatchingLocaleForCode("ar-US"));
  EXPECT_EQ(std::string("ar-BH"), GetMatchingLocaleForCode("ar-BH"));
  EXPECT_EQ(std::string("ar-EG"), GetMatchingLocaleForCode("ar-EG"));
}

// Tests that the Great Britain locale is used for en-GB.
TEST_F(SpeechInputLocaleConfigImplTest, DefaultForGB) {
  EXPECT_EQ(std::string("en-GB"), GetMatchingLocaleForCode("en-GB"));
}

// Tests that the Taiwanese locales are correctly matched.
TEST_F(SpeechInputLocaleConfigImplTest, DefaultForTW) {
  EXPECT_EQ(std::string("zh-Hant-TW"), GetMatchingLocaleForCode("zh-Hant-TW"));
}

// Tests that the display name for the en-US locale is correct.
TEST_F(SpeechInputLocaleConfigImplTest, languageNameFromCodeUS) {
  std::string kEnglishUSDisplayName("English (United States)");
  EXPECT_EQ(base::UTF8ToUTF16(kEnglishUSDisplayName),
            config()->GetLocaleForCode("en-CA").display_name);
  EXPECT_EQ(base::UTF8ToUTF16(kEnglishUSDisplayName),
            config()->GetLocaleForCode("en-US").display_name);
}

// Tests that the display name for the pl-PL locale is correct.
TEST_F(SpeechInputLocaleConfigImplTest, languageNameFromCodePL) {
  std::string kPolishPLDisplayName("Polski (Polska)");
  EXPECT_EQ(base::UTF8ToUTF16(kPolishPLDisplayName),
            config()->GetLocaleForCode("pl-PL").display_name);
}

// Tests that an empty SpeechInputLocale is returned for invalid locale codes.
TEST_F(SpeechInputLocaleConfigImplTest, languageNameFromCodeInvalid) {
  voice::SpeechInputLocale locale = config()->GetLocaleForCode("xx-XX");
  EXPECT_EQ(0U, locale.code.length());
  EXPECT_EQ(0U, locale.display_name.length());
}
