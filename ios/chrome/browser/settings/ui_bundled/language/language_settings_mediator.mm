// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/language/language_settings_mediator.h"

#import <algorithm>
#import <memory>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/i18n/language_tag.h"
#import "base/i18n/tag_converters.h"
#import "base/metrics/histogram_macros.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/language/core/browser/language_model_manager.h"
#import "components/language/core/browser/pref_names.h"
#import "components/language/core/common/locale_util.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/translate/core/browser/translate_pref_names.h"
#import "components/translate/core/browser/translate_prefs.h"
#import "components/translate/core/common/translate_language_matcher.h"
#import "ios/chrome/browser/settings/ui_bundled/language/cells/language_item.h"
#import "ios/chrome/browser/settings/ui_bundled/language/language_settings_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/language/language_settings_histograms.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/translate/model/chrome_ios_translate_client.h"
#import "ios/chrome/browser/translate/model/translate_service_ios.h"
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
      GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
      _translatePrefs->IsTranslateAllowedByPolicy(), &supportedLanguages);
  std::map<std::string, translate::TranslateLanguageInfo> supportedLanguagesMap;
  for (const auto& supportedLanguage : supportedLanguages) {
    supportedLanguagesMap[supportedLanguage.code] = supportedLanguage;
  }

  // Get the accept languages.
  std::vector<base::i18n::LanguageTag> languageTags =
      _translatePrefs->GetLanguageList();

  NSMutableArray<LanguageItem*>* acceptLanguages =
      [NSMutableArray arrayWithCapacity:languageTags.size()];
  for (const auto& languageTag : languageTags) {
    std::string languageCode(languageTag.tag_string());
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
    // format. To convert the former to the latter,
    // `GetTranslateLanguageMatcher()` is used.
    std::string canonicalLanguageCode =
        std::string(translate::GetTranslateLanguageMatcher()
                        .MatchOrDefault(languageItem.languageTag)
                        .tag_string());
    std::string targetLanguageCode = TranslateServiceIOS::GetTargetLanguage(
        self.prefService, self.languageModelManager->GetPrimaryModel());
    languageItem.targetLanguage = targetLanguageCode == canonicalLanguageCode;

    // A language is Translate-blocked if the language is not supported by the
    // Translate server, or user is fluent in the language, or it is the
    // Translate target language.
    languageItem.blocked =
        !languageItem.supportsTranslate ||
        _translatePrefs->IsBlockedLanguage(languageTag.tag_string()) ||
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
  std::vector<base::i18n::LanguageTag> acceptLanguageTags =
      _translatePrefs->GetLanguageList();

  // Get the supported languages.
  std::vector<translate::TranslateLanguageInfo> languages;
  translate::TranslatePrefs::GetLanguageInfoList(
      GetApplicationContext()->GetApplicationLocaleStorage()->Get(),
      _translatePrefs->IsTranslateAllowedByPolicy(), &languages);

  NSMutableArray<LanguageItem*>* supportedLanguages =
      [NSMutableArray arrayWithCapacity:languages.size()];
  for (const auto& language : languages) {
    // Ignore languages already in the accept languages list.
    std::optional<base::i18n::LanguageTag> languageTag =
        base::i18n::GetLanguageTagFromString(language.code);
    if (languageTag &&
        std::ranges::contains(acceptLanguageTags, *languageTag)) {
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

- (void)moveLanguage:(base::i18n::LanguageTag)languageTag
            downward:(BOOL)downward
          withOffset:(NSUInteger)offset {
  translate::TranslatePrefs::RearrangeSpecifier where =
      downward ? translate::TranslatePrefs::kDown
               : translate::TranslatePrefs::kUp;
  std::vector<base::i18n::LanguageTag> languageTags =
      _translatePrefs->GetLanguageList();
  std::vector<std::string> languageCodes;
  languageCodes.reserve(languageTags.size());
  for (const auto& tag : languageTags) {
    languageCodes.push_back(std::string(tag.tag_string()));
  }
  _translatePrefs->RearrangeLanguage(std::string(languageTag.tag_string()),
                                     where, offset, languageCodes);

  UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsActionsHistogram,
                            LanguageSettingsActions::LANGUAGE_LIST_REORDERED);
}

- (void)addLanguage:(base::i18n::LanguageTag)languageTag {
  _translatePrefs->AddToLanguageList(languageTag, /*force_blocked=*/false);

  UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsActionsHistogram,
                            LanguageSettingsActions::LANGUAGE_ADDED);
}

- (void)removeLanguage:(base::i18n::LanguageTag)languageTag {
  _translatePrefs->RemoveFromLanguageList(languageTag);

  UMA_HISTOGRAM_ENUMERATION(kLanguageSettingsActionsHistogram,
                            LanguageSettingsActions::LANGUAGE_REMOVED);
}

- (void)blockLanguage:(base::i18n::LanguageTag)languageTag {
  _translatePrefs->BlockLanguage(languageTag.tag_string());

  UMA_HISTOGRAM_ENUMERATION(
      kLanguageSettingsActionsHistogram,
      LanguageSettingsActions::DISABLE_TRANSLATE_FOR_SINGLE_LANGUAGE);
}

- (void)unblockLanguage:(base::i18n::LanguageTag)languageTag {
  _translatePrefs->UnblockLanguage(languageTag.tag_string());

  UMA_HISTOGRAM_ENUMERATION(
      kLanguageSettingsActionsHistogram,
      LanguageSettingsActions::ENABLE_TRANSLATE_FOR_SINGLE_LANGUAGE);
}

#pragma mark - Private methods

- (LanguageItem*)languageItemFromLanguage:
    (const translate::TranslateLanguageInfo&)language {
  LanguageItem* languageItem = [[LanguageItem alloc] init];
  languageItem.languageTag =
      base::i18n::GetLanguageTagFromString(language.code)
          .value_or(base::i18n::GetKnownLanguageTag("und"));
  languageItem.text = base::SysUTF8ToNSString(language.display_name);
  languageItem.leadingDetailText =
      base::SysUTF8ToNSString(language.native_display_name);
  languageItem.supportsTranslate = language.supports_translate;
  return languageItem;
}

@end
