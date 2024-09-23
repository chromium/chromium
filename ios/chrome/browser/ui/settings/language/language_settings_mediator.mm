// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/language/language_settings_mediator.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/contains.h"
#import "base/metrics/histogram_macros.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/language/core/browser/language_model_manager.h"
#import "components/language/core/browser/pref_names.h"
#import "components/language/core/common/language_util.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/model/translate_service_ios.h"
#import "ios/chrome/browser/ui/settings/language/cells/language_item.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/language/language_settings_histograms.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface LanguageSettingsMediator () <PrefObserverDelegate> {
  // Registrar for pref change notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;

  // Pref observer to track changes to translate::prefs::kOfferTranslateEnabled.
  std::unique_ptr<PrefObserverBridge> _offerTranslatePrefObserverBridge;

  // Pref observer to track changes to language::prefs::kAcceptLanguages.
  std::unique_ptr<PrefObserverBridge> _acceptLanguagesPrefObserverBridge;

  // Pref observer to track changes to prefs::kBlockedLanguages.
  std::unique_ptr<PrefObserverBridge> _blockedLanguagesPrefObserverBridge;

  // Translate wrapper for the PrefService.
  std::unique_ptr<translate::TranslatePrefs> _translatePrefs;
}

// The LanguageModelManager passed to this instance.
@property(nonatomic, assign)
    language::LanguageModelManager* languageModelManager;
// The PrefService passed to this instance.
@property(nonatomic, assign) PrefService* prefService;

@end

@implementation LanguageSettingsMediator

@synthesize consumer = _consumer;

- (instancetype)initWithLanguageModelManager:
                    (language::LanguageModelManager*)languageModelManager
                                 prefService:(PrefService*)prefService {
  self = [super init];
  if (self) {
    _languageModelManager = languageModelManager;
    _prefService = prefService;

    _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
    _prefChangeRegistrar->Init(self.prefService);
    _offerTranslatePrefObserverBridge =
        std::make_unique<PrefObserverBridge>(self);
    _offerTranslatePrefObserverBridge->ObserveChangesForPreference(
        translate::prefs::kOfferTranslateEnabled, _prefChangeRegistrar.get());
    _acceptLanguagesPrefObserverBridge =
        std::make_unique<PrefObserverBridge>(self);
    _acceptLanguagesPrefObserverBridge->ObserveChangesForPreference(
        language::prefs::kAcceptLanguages, _prefChangeRegistrar.get());
    _blockedLanguagesPrefObserverBridge =
        std::make_unique<PrefObserverBridge>(self);
    _blockedLanguagesPrefObserverBridge->ObserveChangesForPreference(
        translate::prefs::kBlockedLanguages, _prefChangeRegistrar.get());

    _translatePrefs =
        ChromeIOSTranslateClient::CreateTranslatePrefs(self.prefService);
  }
  return self;
}

- (void)dealloc {
  // In case this has not been explicitly called.
  [self stopObservingModel];
  _languageModelManager = nullptr;
  _prefService = nullptr;
}

#pragma mark - PrefObserverDelegate

// Called when the value of translate::prefs::kOfferTranslateEnabled,
// language::prefs::kAcceptLanguages or
// translate::prefs::kBlockedLanguages change.
- (void)onPreferenceChanged:(const std::string&)preferenceName {
  DCHECK(preferenceName == translate::prefs::kOfferTranslateEnabled ||
         preferenceName == language::prefs::kAcceptLanguages ||
         preferenceName == translate::prefs::kBlockedLanguages);

  // Inform the consumer.
  if (preferenceName == translate::prefs::kOfferTranslateEnabled) {
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
      // languageCodes comes from a synced pref and may contain language codes
      // that are not supported on the platform, or on this device locale as
      // defined by the GetLanguageInfoList above.
      // Ignore them.
      // TODO(crbug.com/40263219): Investigate why this happens and how to
      // reconcile data.
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
        self.prefService, self.languageModelManager->GetPrimaryModel());
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
    if (base::Contains(acceptLanguageCodes, language.code)) {
      continue;
    }
    LanguageItem* languageItem = [self languageItemFromLanguage:language];
    languageItem.accessibilityTraits |= UIAccessibilityTraitButton;
    [supportedLanguages addObject:languageItem];
  }
  return supportedLanguages;
}

- (BOOL)translateEnabled {
  return self.prefService->GetBoolean(translate::prefs::kOfferTranslateEnabled);
}

- (BOOL)translateManaged {
  return self.prefService->IsManagedPreference(
      translate::prefs::kOfferTranslateEnabled);
}

- (void)stopObservingModel {
  _offerTranslatePrefObserverBridge.reset();
  _acceptLanguagesPrefObserverBridge.reset();
  _blockedLanguagesPrefObserverBridge.reset();
  _prefChangeRegistrar.reset();
  _translatePrefs.reset();
}

#pragma mark - LanguageSettingsCommands

- (void)setTranslateEnabled:(BOOL)enabled {
  self.prefService->SetBoolean(translate::prefs::kOfferTranslateEnabled,
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
