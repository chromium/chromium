// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/terms_util.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

extern std::string GetLocalizedFileName(const std::string& base_name,
                                        const std::string& locale,
                                        const std::string& ext);

extern std::string GetIOSLocaleMapping(const std::string& locale);

namespace {

class FileLocationsTest : public PlatformTest {
 protected:
  void SetUp() override {
    // These files must exist for this unit test to pass.
    termsBaseName_ = "terms";
    extension_ = "html";
    enFile_ = "terms_en.html";
    frFile_ = "terms_fr.html";
  }
  std::string termsBaseName_;
  std::string extension_;
  std::string enFile_;
  std::string frFile_;
};

TEST_F(FileLocationsTest, TestTermsOfServiceUrl) {
  std::string filename(GetTermsOfServicePath());
  EXPECT_FALSE(filename.empty());
}

TEST_F(FileLocationsTest, TestUnifiedTermsOfServiceUrl) {
  GURL embbeded_terms_of_service_url(
      GetUnifiedTermsOfServiceURL(/*embbeded=*/true));
  EXPECT_TRUE(embbeded_terms_of_service_url.is_valid());
  GURL terms_of_service_url = GetUnifiedTermsOfServiceURL(/*embbeded=*/false);
  EXPECT_TRUE(terms_of_service_url.is_valid());
  EXPECT_NE(embbeded_terms_of_service_url, terms_of_service_url);
}

TEST_F(FileLocationsTest, TestIOSLocaleMapping) {
  EXPECT_EQ("en-US", GetIOSLocaleMapping("en-US"));
  EXPECT_EQ("es", GetIOSLocaleMapping("es"));
  EXPECT_EQ("es-419", GetIOSLocaleMapping("es-MX"));
  EXPECT_EQ("pt-BR", GetIOSLocaleMapping("pt"));
  EXPECT_EQ("pt-PT", GetIOSLocaleMapping("pt-PT"));
  EXPECT_EQ("zh-CN", GetIOSLocaleMapping("zh-Hans"));
  EXPECT_EQ("zh-TW", GetIOSLocaleMapping("zh-Hant"));
}

TEST_F(FileLocationsTest, TestFileNameLocaleWithExtension) {
  EXPECT_EQ(enFile_, GetLocalizedFileName(termsBaseName_, "en", extension_));
  EXPECT_EQ(frFile_, GetLocalizedFileName(termsBaseName_, "fr", extension_));
  EXPECT_EQ(frFile_, GetLocalizedFileName(termsBaseName_, "fr-XX", extension_));

  // No ToS for "xx" locale so expect default "en" ToS. Unlikely, but this
  // test will fail once the ToS for "xx" locale is added.
  EXPECT_EQ(enFile_, GetLocalizedFileName(termsBaseName_, "xx", extension_));
}

// Tests that locale/languages available on iOS are mapped to either a
// translated Chrome Terms of Service or to English.
TEST_F(FileLocationsTest, TestTermsOfServiceForSupportedLanguages) {
  // TODO(crbug.com/41195990): List of available localized terms_*.html files.
  // This list is manually maintained as new locales are added to
  // components/resources/terms/.
  NSSet* localizedTermsHtml = [NSSet
      setWithObjects:@"am", @"ar", @"bg", @"bn", @"ca", @"cs", @"da", @"de",
                     @"el", @"en-GB", @"en", @"es-419", @"es", @"et", @"fa",
                     @"fi", @"fil", @"fr", @"gu", @"he", @"hi", @"hr", @"hu",
                     @"id", @"it", @"ja", @"kn", @"ko", @"lt", @"lv", @"ml",
                     @"mr", @"nb", @"nl", @"pl", @"pt-BR", @"pt-PT", @"ro",
                     @"ru", @"sk", @"sl", @"sr", @"sv", @"sw", @"ta", @"te",
                     @"th", @"tr", @"uk", @"vi", @"zh-CN", @"zh-TW", nil];
  // Languages supported by iOS is returned by -availableLocaleIdentifiers.
  // This unit test fails when a language available in iOS falls back to
  // English (en) even though the terms_*.html file is available (listed in
  // `localizedTermsHtml`). Fix this by adding the missing terms_*.html
  // to ios/chrome/browser/shared/ui/util/BUILD.gn
  NSMutableSet<NSString*>* incorrectFallback = [NSMutableSet set];
  for (NSString* locale in [NSLocale availableLocaleIdentifiers]) {
    NSString* normalizedLocale =
        [locale stringByReplacingOccurrencesOfString:@"_" withString:@"-"];
    NSArray* parts = [normalizedLocale componentsSeparatedByString:@"-"];
    NSString* language = [parts objectAtIndex:0];
    std::string filename = GetLocalizedFileName(
        termsBaseName_,
        GetIOSLocaleMapping(base::SysNSStringToUTF8(normalizedLocale)),
        extension_);
    if (filename == enFile_ && ![language isEqualToString:@"en"] &&
        [localizedTermsHtml containsObject:language]) {
      [incorrectFallback addObject:language];
    }
  }
  NSUInteger numberOfMissingFiles = [incorrectFallback count];
  EXPECT_EQ(numberOfMissingFiles, 0U);
  if (numberOfMissingFiles) {
    NSLog(@"Add the following file%@ to ios/chrome/browser/ui/BUILD.gn",
          numberOfMissingFiles > 1 ? @"s" : @"");
    for (NSString* language in incorrectFallback) {
      NSLog(@"  terms_%@.html", language);
    }
  }
}

}  // namespace
