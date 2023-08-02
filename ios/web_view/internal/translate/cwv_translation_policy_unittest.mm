// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_translation_policy.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace ios_web_view {

using CWVTranslationPolicyTest = PlatformTest;

// Tests CWVTranslationPolicy initialization.
TEST_F(CWVTranslationPolicyTest, Initialization) {
  CWVTranslationPolicy* ask_policy =
      [CWVTranslationPolicy translationPolicyAsk];
  EXPECT_EQ(CWVTranslationPolicyAsk, ask_policy.type);

  CWVTranslationPolicy* never_policy =
      [CWVTranslationPolicy translationPolicyNever];
  EXPECT_EQ(CWVTranslationPolicyNever, never_policy.type);

  CWVTranslationLanguage* language = [[CWVTranslationLanguage alloc]
      initWithLanguageCode:base::SysNSStringToUTF8(@"ja")
             localizedName:base::SysNSStringToUTF16(@"Japanese")
                nativeName:base::SysNSStringToUTF16(@"日本語")];
  CWVTranslationPolicy* auto_policy =
      [CWVTranslationPolicy translationPolicyAutoTranslateToLanguage:language];
  EXPECT_EQ(CWVTranslationPolicyAuto, auto_policy.type);
  EXPECT_NSEQ(language, auto_policy.language);
}

}  // namespace ios_web_view
