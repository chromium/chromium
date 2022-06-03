// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language/language_settings_app_interface.h"

#include "base/strings/sys_string_conversions.h"
#include "components/language/core/browser/pref_names.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#import "ios/chrome/test/app/chrome_test_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
std::unique_ptr<translate::TranslatePrefs> CreateTranslatePrefs() {
  return ChromeIOSTranslateClient::CreateTranslatePrefs(
      chrome_test_util::GetOriginalBrowserState()->GetPrefs());
}
}  // namespace

@implementation LanguageSettingsAppInterface : NSObject

+ (void)removeAllLanguages {
  auto translatePrefs = CreateTranslatePrefs();
  std::vector<std::string> languages;
  translatePrefs->GetLanguageList(&languages);
  for (const auto& language : languages) {
    translatePrefs->RemoveFromLanguageList(language);
  }
}

+ (NSString*)languages {
  return base::SysUTF8ToNSString(
      chrome_test_util::GetOriginalBrowserState()->GetPrefs()->GetString(
          language::prefs::kAcceptLanguages));
}

+ (void)addLanguage:(NSString*)language {
  CreateTranslatePrefs()->AddToLanguageList(base::SysNSStringToUTF8(language),
                                            /*force_blocked=*/false);
}

+ (BOOL)offersTranslation {
  return CreateTranslatePrefs()->IsOfferTranslateEnabled();
}

+ (BOOL)isBlockedLanguage:(NSString*)language {
  return CreateTranslatePrefs()->IsBlockedLanguage(
      base::SysNSStringToUTF8(language));
}

+ (void)setRecentTargetLanguage:(NSString*)language {
  return CreateTranslatePrefs()->SetRecentTargetLanguage(
      base::SysNSStringToUTF8(language));
}

@end
