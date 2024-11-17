// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language/language_settings_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "components/language/core/browser/pref_names.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/test/app/chrome_test_util.h"

namespace {
std::unique_ptr<translate::TranslatePrefs> CreateTranslatePrefs() {
  return ChromeIOSTranslateClient::CreateTranslatePrefs(
      chrome_test_util::GetOriginalProfile()->GetPrefs());
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
      chrome_test_util::GetOriginalProfile()->GetPrefs()->GetString(
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
