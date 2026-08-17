// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/language/language_settings_app_interface.h"

#import "base/i18n/language_tag.h"
#import "base/i18n/tag_converters.h"
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
  for (const auto& language : translatePrefs->GetLanguageList()) {
    translatePrefs->RemoveFromLanguageList(language);
  }
}

+ (NSString*)languages {
  return base::SysUTF8ToNSString(
      chrome_test_util::GetOriginalProfile()->GetPrefs()->GetString(
          language::prefs::kAcceptLanguages));
}

+ (void)addLanguage:(NSString*)language {
  std::string language_str = base::SysNSStringToUTF8(language);
  if (std::optional<base::i18n::LanguageTag> parsed_tag =
          base::i18n::GetLanguageTagFromString(language_str)) {
    CreateTranslatePrefs()->AddToLanguageList(*parsed_tag,
                                              /*force_blocked=*/false);
  }
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
