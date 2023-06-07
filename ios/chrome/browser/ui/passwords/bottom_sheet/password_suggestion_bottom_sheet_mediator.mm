// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/core/browser/password_store_interface.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/autofill/manual_fill/passwords_fetcher.h"
#import "ios/chrome/browser/default_browser/utils.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_event.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_protocol.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using PasswordSuggestionBottomSheetExitReason::kDismissal;
using PasswordSuggestionBottomSheetExitReason::kUsePasswordSuggestion;
using ReauthenticationEvent::kAttempt;
using ReauthenticationEvent::kFailure;
using ReauthenticationEvent::kMissingPasscode;
using ReauthenticationEvent::kSuccess;

@interface PasswordSuggestionBottomSheetMediator () <WebStateListObserving,
                                                     CRWWebStateObserver,
                                                     PasswordFetcherDelegate>

// The object that provides suggestions while filling forms.
@property(nonatomic, weak) id<FormInputSuggestionsProvider> suggestionsProvider;

// List of suggestions in the bottom sheet.
@property(nonatomic, strong) NSArray<FormSuggestion*>* suggestions;

// Default globe favicon when no favicon is available.
@property(nonatomic, readonly) FaviconAttributes* defaultGlobeIconAttributes;

// The password fetcher to query the user profile.
@property(nonatomic, strong) PasswordFetcher* passwordFetcher;

@end

@implementation PasswordSuggestionBottomSheetMediator {
  // The interfaces for getting and manipulating a user's saved passwords.
  scoped_refptr<password_manager::PasswordStoreInterface> _profilePasswordStore;
  scoped_refptr<password_manager::PasswordStoreInterface> _accountPasswordStore;

  // Origin to fetch passwords for.
  GURL _URL;

  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<web::WebStateObserverBridge> _observer;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;

  // Vector of credentials related to the current page.
  std::vector<password_manager::CredentialUIEntry> _credentials;

  // Whether the field that triggered the bottom sheet will need to refocus when
  // the bottom sheet is dismissed. Default is true.
  bool _needsRefocus;

  // Web Frame associated with this bottom sheet.
  std::string _frameId;

  // FaviconLoader is a keyed service that uses LargeIconService to retrieve
  // favicon images.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Preference service from the application context.
  PrefService* _prefService;

  // Module containing the reauthentication mechanism.
  __weak id<ReauthenticationProtocol> _reauthenticationModule;
}

@synthesize defaultGlobeIconAttributes = _defaultGlobeIconAttributes;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       faviconLoader:(FaviconLoader*)faviconLoader
                         prefService:(PrefService*)prefService
                              params:(const autofill::FormActivityParams&)params
                        reauthModule:(id<ReauthenticationProtocol>)reauthModule
                                 URL:(const GURL&)URL
                profilePasswordStore:
                    (scoped_refptr<password_manager::PasswordStoreInterface>)
                        profilePasswordStore
                accountPasswordStore:
                    (scoped_refptr<password_manager::PasswordStoreInterface>)
                        accountPasswordStore {
  if (self = [super init]) {
    _needsRefocus = true;
    _frameId = params.frame_id;
    _faviconLoader = faviconLoader;
    _prefService = prefService;
    _reauthenticationModule = reauthModule;

    _profilePasswordStore = profilePasswordStore;
    _accountPasswordStore = accountPasswordStore;
    _URL = URL;

    _webStateList = webStateList;
    web::WebState* activeWebState = _webStateList->GetActiveWebState();

    // Create and register the observers.
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        _webStateList, _observer.get());

    if (activeWebState) {
      FormSuggestionTabHelper* tabHelper =
          FormSuggestionTabHelper::FromWebState(activeWebState);
      DCHECK(tabHelper);

      self.suggestionsProvider = tabHelper->GetAccessoryViewProvider();
      DCHECK(self.suggestionsProvider);

      [self.suggestionsProvider
          retrieveSuggestionsForForm:params
                            webState:activeWebState
            accessoryViewUpdateBlock:^(
                NSArray<FormSuggestion*>* suggestions,
                id<FormInputSuggestionsProvider> formInputSuggestionsProvider) {
              self.suggestions = suggestions;
            }];

      // Fetch passwords related to the suggestions.
      _credentials.clear();
      self.passwordFetcher = [[PasswordFetcher alloc]
          initWithProfilePasswordStore:_profilePasswordStore
                  accountPasswordStore:_accountPasswordStore
                              delegate:self
                                   URL:url::Origin::Create(_URL).GetURL()];
    }
  }
  return self;
}

- (void)dealloc {
}

- (void)disconnect {
  _prefService = nullptr;
  _faviconLoader = nullptr;
  _forwarder = nullptr;
  _observer = nullptr;
  _webStateList = nullptr;
}

- (BOOL)hasSuggestions {
  return [self.suggestions count] > 0;
}

- (absl::optional<password_manager::CredentialUIEntry>)
    getCredentialForFormSuggestion:(FormSuggestion*)formSuggestion {
  NSString* username = formSuggestion.value;
  if ([username containsString:kPasswordFormSuggestionSuffix]) {
    username = [username
        stringByReplacingOccurrencesOfString:kPasswordFormSuggestionSuffix
                                  withString:@""];
  }
  auto it = base::ranges::find_if(
      _credentials,
      [username](const password_manager::CredentialUIEntry& credential) {
        CHECK(!credential.facets.empty());
        for (auto facet : credential.facets) {
          if ([base::SysUTF16ToNSString(credential.username)
                  isEqualToString:username]) {
            return true;
          }
        }
        return false;
      });
  return it != _credentials.end()
             ? absl::optional<password_manager::CredentialUIEntry>(*it)
             : absl::nullopt;
}

- (void)logExitReason:(PasswordSuggestionBottomSheetExitReason)exitReason {
  base::UmaHistogramEnumeration("IOS.PasswordBottomSheet.ExitReason",
                                exitReason);
}

#pragma mark - Accessors

- (void)setConsumer:(id<PasswordSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;
  if ([self hasSuggestions]) {
    NSString* domain = @"";
    if (!_URL.is_empty()) {
      url::Origin origin = url::Origin::Create(_URL);
      domain =
          base::SysUTF8ToNSString(password_manager::GetShownOrigin(origin));
    }
    [consumer setSuggestions:self.suggestions andDomain:domain];
  } else {
    [consumer dismiss];
  }
}

#pragma mark - PasswordSuggestionBottomSheetDelegate

- (void)didSelectSuggestion:(NSInteger)row {
  DCHECK(row >= 0);

  FormSuggestion* suggestion = [self.suggestions objectAtIndex:row];

  [self logExitReason:kUsePasswordSuggestion];
  [self logReauthEvent:kAttempt];

  if (!suggestion.requiresReauth) {
    [self logReauthEvent:kSuccess];
    [self selectSuggestion:suggestion];
    return;
  }
  if ([_reauthenticationModule canAttemptReauth]) {
    __weak __typeof(self) weakSelf = self;
    auto completionHandler = ^(ReauthenticationResult result) {
      [weakSelf selectSuggestion:suggestion reauthenticationResult:result];
    };

    NSString* reason = l10n_util::GetNSString(IDS_IOS_AUTOFILL_REAUTH_REASON);
    [_reauthenticationModule
        attemptReauthWithLocalizedReason:reason
                    canReusePreviousAuth:YES
                                 handler:completionHandler];
  } else {
    [self logReauthEvent:kMissingPasscode];
    [self selectSuggestion:suggestion];
  }
}

- (void)refocus {
  if (_needsRefocus && _webStateList) {
    [self logExitReason:kDismissal];
    [self incrementDismissCount];

    web::WebState* activeWebState = _webStateList->GetActiveWebState();
    password_manager::PasswordManagerJavaScriptFeature* feature =
        password_manager::PasswordManagerJavaScriptFeature::GetInstance();
    web::WebFramesManager* framesManager =
        feature->GetWebFramesManager(activeWebState);
    if (framesManager) {
      web::WebFrame* frame = framesManager->GetFrameWithId(_frameId);
      AutofillBottomSheetTabHelper::FromWebState(activeWebState)
          ->DetachListenersAndRefocus(frame);
      [self disconnect];
    }
  }
}

// Disables future refocus requests.
- (void)disableRefocus {
  _needsRefocus = false;
}

- (void)loadFaviconWithBlockHandler:
    (FaviconLoader::FaviconAttributesCompletionBlock)faviconLoadedBlock {
  CHECK(_faviconLoader);
  if (!_URL.is_empty()) {
    _faviconLoader->FaviconForPageUrl(
        _URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
        /*fallback_to_google_server=*/NO, faviconLoadedBlock);
  } else {
    faviconLoadedBlock([self defaultGlobeIconAttributes]);
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_EQ(_webStateList, webStateList);
  if (atIndex == webStateList->active_index()) {
    [self disableRefocus];
    [self.consumer dismiss];
  }
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  DCHECK_EQ(_webStateList, webStateList);
  [self disableRefocus];
  [self.consumer dismiss];
}

- (void)webStateListDestroyed:(WebStateList*)webStateList {
  DCHECK_EQ(webStateList, _webStateList);
  _forwarder = nullptr;
  _observer = nullptr;
  _webStateList = nullptr;
  [self disableRefocus];
  [self.consumer dismiss];
}

#pragma mark - CRWWebStateObserver

- (void)webStateDestroyed:(web::WebState*)webState {
  [self disableRefocus];
  [self.consumer dismiss];
}

- (void)webState:(web::WebState*)webState
    didFinishNavigation:(web::NavigationContext*)navigation {
  [self disableRefocus];
  [self.consumer dismiss];
}

- (void)renderProcessGoneForWebState:(web::WebState*)webState {
  [self disableRefocus];
  [self.consumer dismiss];
}

#pragma mark - PasswordFetcherDelegate

- (void)passwordFetcher:(PasswordFetcher*)passwordFetcher
      didFetchPasswords:
          (std::vector<std::unique_ptr<password_manager::PasswordForm>>)
              passwords {
  std::vector<password_manager::CredentialUIEntry> credentials;
  for (const auto& form : passwords) {
    credentials.push_back(password_manager::CredentialUIEntry(*form));
  }
  _credentials = credentials;
}

#pragma mark - Private

// Perform suggestion selection
- (void)selectSuggestion:(FormSuggestion*)suggestion {
  LogLikelyInterestedDefaultBrowserUserActivity(DefaultPromoTypeStaySafe);
  [self.suggestionsProvider didSelectSuggestion:suggestion];
  [self disconnect];
}

// Perform suggestion selection based on the reauthentication result.
- (void)selectSuggestion:(FormSuggestion*)suggestion
    reauthenticationResult:(ReauthenticationResult)result {
  if (result != ReauthenticationResult::kFailure) {
    [self logReauthEvent:kSuccess];
    [self selectSuggestion:suggestion];
  } else {
    [self logReauthEvent:kFailure];
    [self disconnect];
  }
}

// Returns the default favicon attributes after making sure they are
// initialized.
- (FaviconAttributes*)defaultGlobeIconAttributes {
  if (!_defaultGlobeIconAttributes) {
    _defaultGlobeIconAttributes = [FaviconAttributes
        attributesWithImage:DefaultSymbolWithPointSize(
                                kGlobeAmericasSymbol,
                                kDesiredMediumFaviconSizePt)];
  }
  return _defaultGlobeIconAttributes;
}

// Increments the dismiss count preference.
- (void)incrementDismissCount {
  if (_prefService) {
    int currentDismissCount =
        _prefService->GetInteger(prefs::kIosPasswordBottomSheetDismissCount);
    if (currentDismissCount <
        AutofillBottomSheetTabHelper::kPasswordBottomSheetMaxDismissCount) {
      _prefService->SetInteger(prefs::kIosPasswordBottomSheetDismissCount,
                               currentDismissCount + 1);
    }
  }
}

// Logs reauthentication events.
- (void)logReauthEvent:(ReauthenticationEvent)event {
  base::UmaHistogramEnumeration("IOS.Reauth.Password.BottomSheet", event);
}

@end
