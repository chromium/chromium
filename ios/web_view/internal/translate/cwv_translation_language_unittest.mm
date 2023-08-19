// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace ios_web_view {

using CWVTranslationLanguageTest = PlatformTest;

// Tests CWVTranslationLanguage initialization.
TEST_F(CWVTranslationLanguageTest, Initialization) {
  NSString* language_code = @"ja";
  NSString* localized_name = @"Japanese";
  NSString* native_name = @"日本語";
  CWVTranslationLanguage* language = [[CWVTranslationLanguage alloc]
      initWithLanguageCode:base::SysNSStringToUTF8(language_code)
             localizedName:base::SysNSStringToUTF16(localized_name)
                nativeName:base::SysNSStringToUTF16(native_name)];

  EXPECT_NSEQ(language_code, language.languageCode);
  EXPECT_NSEQ(localized_name, language.localizedName);
  EXPECT_NSEQ(native_name, language.nativeName);
}

TEST_F(CWVTranslationLanguageTest, Equality) {
  // Two languages with the same langauge code but different localized/native
  // names.
  CWVTranslationLanguage* language_a = [[CWVTranslationLanguage alloc]
      initWithLanguageCode:"ja"
             localizedName:base::SysNSStringToUTF16(@"JapaneseA")
                nativeName:base::SysNSStringToUTF16(@"日本語A")];
  CWVTranslationLanguage* language_b = [[CWVTranslationLanguage alloc]
      initWithLanguageCode:"ja"
             localizedName:base::SysNSStringToUTF16(@"JapaneseB")
                nativeName:base::SysNSStringToUTF16(@"日本語B")];

  // Equality should only be based on the language code.
  EXPECT_NSEQ(language_a, language_b);
  EXPECT_EQ(language_a.hash, language_b.hash);
}

}  // namespace ios_web_view
