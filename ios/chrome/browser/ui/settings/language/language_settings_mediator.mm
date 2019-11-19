// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language/language_settings_mediator.h"

#include <memory>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/language_util.h"
#include "components/prefs/ios/pref_observer_bridge.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/language/language_model_manager_factory.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#include "ios/chrome/browser/translate/translate_service_ios.h"
#import "ios/chrome/browser/ui/settings/language/cells/language_item.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_histograms.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface LanguageSettingsMediator () <PrefObserverDelegate> {
  // Registrar for pref change notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;

  // Pref observer to track changes to prefs::kOfferTranslateEnabled.
  std::unique_ptr<PrefObserverBridge> _offerTranslatePrefObserverBridge;

  // Pref observer to track changes to language::prefs::kAcceptLanguages.
  std::unique_ptr<PrefObserverBridge> _acceptLanguagesPrefObserverBridge;

  // Pref observer to track changes to language::prefs::kFluentLanguages.
  std::unique_ptr<PrefObserverBridge> _fluentLanguagesPrefObserverBridge;

  // Translate wrapper for the PrefService.
  std::unique_ptr<translate::TranslatePrefs> _translatePrefs;
}

// The BrowserState passed to this instance.
@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

@end

@implementation LanguageSettingsMediator

@synthesize consumer = _consumer;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super init];
  if (self) {
    _browserState = browserState;

    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(browserState->GetPrefs());
    _offerTranslatePrefObserverBridge =
        std::make_unique<PrefObserverBridge>(self);
    _offerTranslatePrefObserverBridge->ObserveChangesForPreference(
        prefs::kOfferTranslateEnabled, _prefChangeRegistrar.get());
    _acceptLanguagesPrefObserverBridge =
        std::make_unique<PrefObserverBridge>(self);
    _acceptLanguagesPrefObserverBridge->ObserveChangesForPreference(
        language::prefs::kAcceptLanguages, _prefChangeRegistrar.get());
    _fluentLanguagesPrefObserverBridge =
        std::make_unique<PrefObserverBridge>(self);
    _fluentLanguagesPrefObserverBridge->ObserveChangesForPreference(
        language::prefs::kFluentLanguages, _prefChangeRegistrar.get());

    _translatePrefs = ChromeIOSTranslateClient::CreateTranslatePrefs(
        browserState->GetPrefs());
  }
  return self;
}

- (void)dealloc {
  // In case this has not been explicitly called.
  [self stopObservingModel];
}

#pragma mark - PrefObserverDelegate

// Called when the value of prefs::kOfferTranslateEnabled,
// language::prefs::kAcceptLanguages or
// language::prefs::kFluentLanguages change.
- (void)onPreferenceChanged:(const std::string&)preferenceName {
  DCHECK(preferenceName == prefs::kOfferTranslateEnabled ||
         preferenceName == language::prefs::kAcceptLanguages ||
         preferenceName == language::prefs::kFluentLanguages);

  // Inform the consumer.
  if (preferenceName == prefs::kOfferTranslateEnabled) {
    [self.consumer translateEnabled:[self translateEnabled]];
  } else {
    [self.consumer languagePrefsChanged];
  }
}

#pragma mark - LanguageSettingsDataSource

- (NSArray<LanguageItem*>*)acceptLanguagesItems {
  // Create a map of supported language codes to supported languages.
  std::vector<translate::TranslateLanguageInfo> supportedLanguages;
  translate::TranslatePrefs::GetLanguageInfoList(
      GetApplicationContext()->GetApplicationLocale(),
      _translatePrefs->IsTranslateAllowedByPolicy(), &supportedLanguages);
  std::map<std::string, translate::TranslateLanguageInfo> supportedLanguagesMap;
  for (const auto& supportedLanguage : supportedLanguages) {
    supportedLanguagesMap[supportedLanguage.code] = supportedLanguage;
  }

  // Get the accept languages.
  std::vector<std::string> languageCodes;
  _translatePrefs->GetLanguageList(&languageCodes);

  NSMutableArray<LanguageItem*>* acceptLanguages =
      [NSMutableArray arrayWithCapacity:languageCodes.size()];
  for (const auto& languageCode : languageCodes) {
    // Ignore unsupported languages.
    auto it = supportedLanguagesMap.find(languageCode);
    if (it == supportedLanguagesMap.end()) {
      NOTREACHED() << languageCode + " is an accept language which is not "
                                     "supported by the platform.";
      continue;
    }
    const translate::TranslateLanguageInfo& language = it->second;
    LanguageItem* languageItem = [self languageItemFromLanguage:language];

    // Language codes used in the language settings have the Chrome internal
    // format while the Translate target language has the Translate server
    // format. To convert the former to the latter the utilily function
    // ToTranslateLanguageSynonym() must be used.
    std::string canonicalLanguageCode = languageItem.languageCode;
    language::ToTranslateLanguageSynonym(&canonicalLanguageCode);
    std::string targetLanguageCode = TranslateServiceIOS::GetTargetLanguage(
        self.browserState->GetPrefs(),
        LanguageModelManagerFactory::GetForBrowserState(self.browserState)
            ->GetPrimaryModel());
    languageItem.targetLanguage = targetLanguageCode == canonicalLanguageCode;

    // A language is Translate-blocked if the language is not supported by the
    // Translate server, or user is fluent in the language, or it is the
    // Translate target language.
    languageItem.blocked =
        !languageItem.supportsTranslate ||
        _translatePrefs->IsBlockedLanguage(languageItem.languageCode) ||
        [languageItem isTargetLanguage];

    if ([self translateEnabled]) {
      // Show a disclosure indicator to suggest language details are available
      // as well as a label indicating if the language is Translate-blocked.
      languageItem.accessibilityTraits |= UIAccessibilityTraitButton;
      languageItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
      languageItem.trailingDetailText =
          [languageItem isBlocked]
              ? l10n_util::GetNSString(
                    IDS_IOS_LANGUAGE_SETTINGS_NEVER_TRANSLATE_TITLE)
              : l10n_util::GetNSString(
                    IDS_IOS_LANGUAGE_SETTINGS_OFFER_TO_TRANSLATE_TITLE);
    }
    [acceptLanguages addObject:languageItem];
  }
  return acceptLanguages;
}

- (NSArray<LanguageItem*>*)supportedLanguagesItems {
  // Get the accept languages.
  std::vector<std::string> acceptLanguageCodes;
  _translatePrefs->GetLanguageList(&acceptLanguageCodes);

  // Get the supported languages.
  std::vector<translate::TranslateLanguageInfo> languages;
  translate::TranslatePrefs::GetLanguageInfoList(
      GetApplicationContext()->GetApplicationLocale(),
      _translatePrefs->IsTranslateAllowedByPolicy(), &languages);

  NSMutableArray<LanguageItem*>* supportedLanguages =
      [NSMutableArray arrayWithCapacity:languages.size()];
  for (const auto& language : languages) {
    // Ignore languages already in the accept languages list.
    if (std::find(acceptLanguageCodes.begin(), acceptLanguageCodes.end(),
                  language.code) != acceptLanguageCodes.end()) {
      continue;
    }
    LanguageItem* languageItem = [self languageItemFromLanguage:language];
    languageItem.accessibilityTraits |= UIAccessibilityTraitButton;
    [supportedLanguages addObject:languageItem];
  }
  return supportedLanguages;
}

- (BOOL)translateEnabled {
  return self.browserState->GetPrefs()->GetBoolean(
      prefs::kOfferTranslateEnabled);
}

- (void)stopObservingModel {
  _offerTranslatePrefObserverBridge.reset();
  _acceptLanguagesPrefObserverBridge.reset();
  _fluentLanguagesPrefObserverBridge.reset();
  _prefChangeRegistrar.reset();
}

#pragma mark - LanguageSettingsCommands

- (void)setTranslateEnabled:(BOOL)enabled {
  self.browserState->GetPrefs()->SetBoolean(prefs::kOfferTranslateEnabled,
                                            enabled);

  UMA_HISTOGRAM_ENUMERATION(
      kLanguageSettingsActionsHistogram,
      enabled ? LanguageSettingsActions::ENABLE_TRANSLATE_GLOBALLY
              : LanguageSettingsActions::DISABLE_TRANSLATE_GLOBALLY);
}

- (void)moveLanguage:(const std::string&)languageCode
            downward:(BOOL)downward
          withOffset:(NSUInteger)offset {
  translate::TranslatePrefs::RearrangeSpecifier where =
      downward ? translate::TranslatePrefs::kDown
               : translate::TranslatePrefs::kUp;
  std::vector<std::string> languageCodes;
  _translatePrefs->GetLanguageList(&languageCodes);
  _translatePrefs->RearrangeLanguage(languageCode, where, offset,
                                     languageCodes);

  UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsActionsHistogram,
                            LanguageSettingsActions::LANGUAGE_LIST_REORDERED);
}

- (void)addLanguage:(const std::string&)languageCode {
  _translatePrefs->AddToLanguageList(languageCode, /*force_blocked=*/false);

  UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsActionsHistogram,
                            LanguageSettingsActions::LANGUAGE_ADDED);
}

- (void)removeLanguage:(const std::string&)languageCode {
  _translatePrefs->RemoveFromLanguageList(languageCode);

  UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsActionsHistogram,
                            LanguageSettingsActions::LANGUAGE_REMOVED);
}

- (void)blockLanguage:(const std::string&)languageCode {
  _translatePrefs->BlockLanguage(languageCode);

  UMA_HISTOGRAM_ENUMERATION(
      kLanguageSettingsActionsHistogram,
      LanguageSettingsActions::DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE);
}

- (void)unblockLanguage:(const std::string&)languageCode {
  _translatePrefs->UnblockLanguage(languageCode);

  UMA_HISTOGRAM_ENUMERATION(
      kLanguageSettingsActionsHistogram,
      LanguageSettingsActions::ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE);
}

#pragma mark - Private methods

- (LanguageItem*)languageItemFromLanguage:
    (const translate::TranslateLanguageInfo&)language {
  LanguageItem* languageItem = [[LanguageItem alloc] init];
  languageItem.languageCode = language.code;
  languageItem.text = base::SysUTF8ToNSString(language.display_name);
  languageItem.leadingDetailText =
      base::SysUTF8ToNSString(language.native_display_name);
  languageItem.supportsTranslate = language.supports_translate;
  return languageItem;
}

@end
