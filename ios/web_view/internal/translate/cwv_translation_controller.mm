// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_translation_controller_internal.h"

#import <memory>
#import <string>

#import "base/check_op.h"
#import "base/memory/ptr_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/translate/core/browser/translate_download_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"
#import "ios/web_view/internal/translate/web_view_translate_client.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_translation_controller_delegate.h"
#import "ios/web_view/public/cwv_translation_policy.h"
#import "ui/base/l10n/l10n_util.h"

NSErrorDomain const CWVTranslationErrorDomain =
    @"org.chromium.chromewebview.TranslationErrorDomain";

namespace {
// Converts a |translate::TranslateErrors| to a |CWVTranslationError|.
CWVTranslationError CWVConvertTranslateError(translate::TranslateErrors type) {
  switch (type) {
    case translate::TranslateErrors::NONE:
      return CWVTranslationErrorNone;
    case translate::TranslateErrors::NETWORK:
      return CWVTranslationErrorNetwork;
    case translate::TranslateErrors::INITIALIZATION_ERROR:
      return CWVTranslationErrorInitializationError;
    case translate::TranslateErrors::UNKNOWN_LANGUAGE:
      return CWVTranslationErrorUnknownLanguage;
    case translate::TranslateErrors::UNSUPPORTED_LANGUAGE:
      return CWVTranslationErrorUnsupportedLanguage;
    case translate::TranslateErrors::IDENTICAL_LANGUAGES:
      return CWVTranslationErrorIdenticalLanguages;
    case translate::TranslateErrors::TRANSLATION_ERROR:
      return CWVTranslationErrorTranslationError;
    case translate::TranslateErrors::TRANSLATION_TIMEOUT:
      return CWVTranslationErrorTranslationTimeout;
    case translate::TranslateErrors::UNEXPECTED_SCRIPT_ERROR:
      return CWVTranslationErrorUnexpectedScriptError;
    case translate::TranslateErrors::BAD_ORIGIN:
      return CWVTranslationErrorBadOrigin;
    case translate::TranslateErrors::SCRIPT_LOAD_ERROR:
      return CWVTranslationErrorScriptLoadError;
    case translate::TranslateErrors::TRANSLATE_ERROR_MAX:
      NOTREACHED_IN_MIGRATION();
      return CWVTranslationErrorNone;
  }
}
}  // namespace

@interface CWVTranslationController () <CRWWebStateObserver>

// A map of CWTranslationLanguages keyed by its language code. Lazily loaded.
@property(nonatomic, readonly)
    NSDictionary<NSString*, CWVTranslationLanguage*>* supportedLanguagesByCode;

// Convenience method to get the language with |languageCode|.
- (CWVTranslationLanguage*)languageWithCode:(const std::string&)languageCode;

@end

@implementation CWVTranslationController {
  web::WebState* _webState;
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<ios_web_view::WebViewTranslateClient> _translateClient;
  std::unique_ptr<translate::TranslatePrefs> _translatePrefs;
  base::Time _languagesLastUpdatedTime;
}

@synthesize delegate = _delegate;
@synthesize supportedLanguagesByCode = _supportedLanguagesByCode;

#pragma mark - Internal Methods

- (instancetype)initWithWebState:(web::WebState*)webState
                 translateClient:
                     (std::unique_ptr<ios_web_view::WebViewTranslateClient>)
                         translateClient {
  self = [super init];
  if (self) {
    _webState = webState;

    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _webState->AddObserver(_webStateObserverBridge.get());

    _translateClient = std::move(translateClient);
    _translateClient->set_translation_controller(self);

    _translatePrefs = _translateClient->GetTranslatePrefs();
  }
  return self;
}

- (void)dealloc {
  if (_webState) {
    _webState->RemoveObserver(_webStateObserverBridge.get());
  }
}

- (void)updateTranslateStep:(translate::TranslateStep)step
             sourceLanguage:(const std::string&)sourceLanguage
             targetLanguage:(const std::string&)targetLanguage
                  errorType:(translate::TranslateErrors)errorType
          triggeredFromMenu:(bool)triggeredFromMenu {
  if (_webState->IsBeingDestroyed()) {
    return;
  }

  CWVTranslationLanguage* source = [self languageWithCode:sourceLanguage];
  CWVTranslationLanguage* target = [self languageWithCode:targetLanguage];

  NSError* error;
  if (errorType != translate::TranslateErrors::NONE) {
    error = [NSError errorWithDomain:CWVTranslationErrorDomain
                                code:CWVConvertTranslateError(errorType)
                            userInfo:nil];
  }

  switch (step) {
    case translate::TRANSLATE_STEP_BEFORE_TRANSLATE: {
      if ([_delegate respondsToSelector:@selector
                     (translationController:canOfferTranslationFromLanguage
                                              :toLanguage:)]) {
        [_delegate translationController:self
            canOfferTranslationFromLanguage:source
                                 toLanguage:target];
      }
      break;
    }
    case translate::TRANSLATE_STEP_TRANSLATING:
      if ([_delegate respondsToSelector:@selector
                     (translationController:didStartTranslationFromLanguage
                                              :toLanguage:userInitiated:)]) {
        [_delegate translationController:self
            didStartTranslationFromLanguage:source
                                 toLanguage:target
                              userInitiated:triggeredFromMenu];
      }
      break;
    case translate::TRANSLATE_STEP_AFTER_TRANSLATE:
    case translate::TRANSLATE_STEP_TRANSLATE_ERROR:
      if ([_delegate respondsToSelector:@selector
                     (translationController:didFinishTranslationFromLanguage
                                              :toLanguage:error:)]) {
        [_delegate translationController:self
            didFinishTranslationFromLanguage:source
                                  toLanguage:target
                                       error:error];
      }
      break;
    case translate::TRANSLATE_STEP_NEVER_TRANSLATE:
      // Not supported.
      break;
  }
}

#pragma mark - Public Methods

- (NSSet*)supportedLanguages {
  return [NSSet setWithArray:self.supportedLanguagesByCode.allValues];
}

- (void)translatePageFromLanguage:(CWVTranslationLanguage*)sourceLanguage
                       toLanguage:(CWVTranslationLanguage*)targetLanguage
                    userInitiated:(BOOL)userInitiated {
  std::string sourceLanguageCode =
      base::SysNSStringToUTF8(sourceLanguage.languageCode);
  std::string targetLanguageCode =
      base::SysNSStringToUTF8(targetLanguage.languageCode);
  _translateClient->TranslatePage(sourceLanguageCode, targetLanguageCode,
                                  userInitiated);
}

- (void)revertTranslation {
  _translateClient->RevertTranslation();
}

- (BOOL)requestTranslationOffer {
  return _translateClient->RequestTranslationOffer();
}

- (void)setTranslationPolicy:(CWVTranslationPolicy*)policy
             forPageLanguage:(CWVTranslationLanguage*)pageLanguage {
  std::string languageCode = base::SysNSStringToUTF8(pageLanguage.languageCode);
  switch (policy.type) {
    case CWVTranslationPolicyAsk: {
      _translatePrefs->UnblockLanguage(languageCode);
      _translatePrefs->RemoveLanguagePairFromAlwaysTranslateList(languageCode);
      break;
    }
    case CWVTranslationPolicyNever: {
      _translatePrefs->AddToLanguageList(languageCode,
                                         /*force_blocked=*/true);
      break;
    }
    case CWVTranslationPolicyAuto: {
      _translatePrefs->UnblockLanguage(languageCode);
      _translatePrefs->AddLanguagePairToAlwaysTranslateList(
          languageCode, base::SysNSStringToUTF8(policy.language.languageCode));
      break;
    }
  }
}

- (CWVTranslationPolicy*)translationPolicyForPageLanguage:
    (CWVTranslationLanguage*)pageLanguage {
  std::string languageCode = base::SysNSStringToUTF8(pageLanguage.languageCode);
  bool isLanguageBlocked = _translatePrefs->IsBlockedLanguage(languageCode);
  if (isLanguageBlocked) {
    return [CWVTranslationPolicy translationPolicyNever];
  }

  std::string autoTargetLanguageCode;
  if (!_translatePrefs->ShouldAutoTranslate(languageCode,
                                            &autoTargetLanguageCode)) {
    return [CWVTranslationPolicy translationPolicyAsk];
  }

  CWVTranslationLanguage* targetLanguage =
      [self languageWithCode:autoTargetLanguageCode];
  return [CWVTranslationPolicy
      translationPolicyAutoTranslateToLanguage:targetLanguage];
}

- (void)setTranslationPolicy:(CWVTranslationPolicy*)policy
                 forPageHost:(NSString*)pageHost {
  DCHECK(pageHost.length);
  switch (policy.type) {
    case CWVTranslationPolicyAsk: {
      _translatePrefs->RemoveSiteFromNeverPromptList(
          base::SysNSStringToUTF8(pageHost));
      break;
    }
    case CWVTranslationPolicyNever: {
      _translatePrefs->AddSiteToNeverPromptList(
          base::SysNSStringToUTF8(pageHost));
      break;
    }
    case CWVTranslationPolicyAuto: {
      // TODO(crbug.com/41310094): Support auto translation policies for
      // websites.
      NOTREACHED_IN_MIGRATION();
      break;
    }
  }
}

- (CWVTranslationPolicy*)translationPolicyForPageHost:(NSString*)pageHost {
  // TODO(crbug.com/41310094): Return translationPolicyAuto when implemented.
  bool isSiteOnNeverPromptList = _translatePrefs->IsSiteOnNeverPromptList(
      base::SysNSStringToUTF8(pageHost));
  if (isSiteOnNeverPromptList) {
    return [CWVTranslationPolicy translationPolicyNever];
  }
  return [CWVTranslationPolicy translationPolicyAsk];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  DCHECK_EQ(_webState, webState);
  _translateClient.reset();
  _translatePrefs.reset();
  _webState->RemoveObserver(_webStateObserverBridge.get());
  _webStateObserverBridge.reset();
  _webState = nullptr;
}

#pragma mark - Private Methods

- (NSDictionary<NSString*, CWVTranslationLanguage*>*)supportedLanguagesByCode {
  base::Time languagesLastUpdatedTime =
      translate::TranslateDownloadManager::GetSupportedLanguagesLastUpdated();
  if (!_supportedLanguagesByCode ||
      _languagesLastUpdatedTime < languagesLastUpdatedTime) {
    NSMutableDictionary<NSString*, CWVTranslationLanguage*>*
        supportedLanguagesByCode = [NSMutableDictionary dictionary];
    std::vector<std::string> languageCodes;
    translate::TranslateDownloadManager::GetSupportedLanguages(
        _translatePrefs->IsTranslateAllowedByPolicy(), &languageCodes);
    std::string locale = translate::TranslateDownloadManager::GetInstance()
                             ->application_locale();
    for (const std::string& languageCode : languageCodes) {
      std::u16string localizedName =
          l10n_util::GetDisplayNameForLocale(languageCode, locale, true);
      std::u16string nativeName =
          l10n_util::GetDisplayNameForLocale(languageCode, languageCode, true);
      CWVTranslationLanguage* language =
          [[CWVTranslationLanguage alloc] initWithLanguageCode:languageCode
                                                 localizedName:localizedName
                                                    nativeName:nativeName];

      supportedLanguagesByCode[language.languageCode] = language;
    }
    _languagesLastUpdatedTime = languagesLastUpdatedTime;
    _supportedLanguagesByCode = [supportedLanguagesByCode copy];
  }
  return _supportedLanguagesByCode;
}

- (CWVTranslationLanguage*)languageWithCode:(const std::string&)languageCode {
  NSString* languageCodeString = base::SysUTF8ToNSString(languageCode);
  return self.supportedLanguagesByCode[languageCodeString];
}

@end
