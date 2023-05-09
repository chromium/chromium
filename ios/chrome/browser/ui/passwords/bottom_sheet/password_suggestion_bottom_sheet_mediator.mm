// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/autofill/bottom_sheet/bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/form_input_suggestions_provider.h"
#import "ios/chrome/browser/autofill/form_suggestion_tab_helper.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/passwords/bottom_sheet/password_suggestion_bottom_sheet_consumer.h"
#import "ios/chrome/browser/ui/settings/password/saved_passwords_presenter_observer.h"
#import "ios/chrome/browser/web_state_list/active_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordSuggestionBottomSheetMediator () <
    WebStateListObserving,
    CRWWebStateObserver,
    SavedPasswordsPresenterObserver>

// The object that provides suggestions while filling forms.
@property(nonatomic, weak) id<FormInputSuggestionsProvider> suggestionsProvider;

// List of suggestions in the bottom sheet.
@property(nonatomic, strong) NSArray<FormSuggestion*>* suggestions;

// Default globe favicon when no favicon is available.
@property(nonatomic, readonly) FaviconAttributes* defaultGlobeIconAttributes;

@end

@implementation PasswordSuggestionBottomSheetMediator {
  // The WebStateList observed by this mediator and the observer bridge.
  raw_ptr<WebStateList> _webStateList;
  std::unique_ptr<web::WebStateObserverBridge> _observer;
  std::unique_ptr<ActiveWebStateObservationForwarder> _forwarder;

  // A helper object for passing data about saved passwords from a finished
  // password store request to the PasswordSuggestionBottomSheetViewController.
  std::unique_ptr<SavedPasswordsPresenterObserverBridge>
      _passwordsPresenterObserver;

  // Service which gives us a view on users' saved passwords.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;

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
}

@synthesize defaultGlobeIconAttributes = _defaultGlobeIconAttributes;

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       faviconLoader:(FaviconLoader*)faviconLoader
                         prefService:(PrefService*)prefService
                              params:(const autofill::FormActivityParams&)params
             savedPasswordsPresenter:
                 (raw_ptr<password_manager::SavedPasswordsPresenter>)
                     passwordPresenter {
  if (self = [super init]) {
    _needsRefocus = true;
    _frameId = params.frame_id;
    _faviconLoader = faviconLoader;
    _prefService = prefService;

    _webStateList = webStateList;
    web::WebState* activeWebState = _webStateList->GetActiveWebState();

    // Create and register the observers.
    _observer = std::make_unique<web::WebStateObserverBridge>(self);
    _forwarder = std::make_unique<ActiveWebStateObservationForwarder>(
        webStateList, _observer.get());

    _savedPasswordsPresenter = passwordPresenter;
    _passwordsPresenterObserver =
        std::make_unique<SavedPasswordsPresenterObserverBridge>(
            self, _savedPasswordsPresenter);
    _savedPasswordsPresenter->Init();

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
    }
  }
  return self;
}

- (void)dealloc {
}

- (void)disconnect {
  DCHECK(_savedPasswordsPresenter);
  DCHECK(_passwordsPresenterObserver);
  _savedPasswordsPresenter->RemoveObserver(_passwordsPresenterObserver.get());
  _passwordsPresenterObserver.reset();
  _prefService = nullptr;
  _faviconLoader = nullptr;
  _forwarder = nullptr;
  _observer = nullptr;
  _webStateList = nullptr;
}

- (BOOL)hasSuggestions {
  return [self.suggestions count] > 0;
}

#pragma mark - Accessors

- (void)setConsumer:(id<PasswordSuggestionBottomSheetConsumer>)consumer {
  _consumer = consumer;
  if ([self hasSuggestions]) {
    [consumer setSuggestions:self.suggestions];
  } else {
    [consumer dismiss];
  }
}

#pragma mark - PasswordSuggestionBottomSheetDelegate

- (void)didSelectSuggestion:(NSInteger)row {
  DCHECK(row >= 0);

  FormSuggestion* suggestion = [self.suggestions objectAtIndex:row];
  [self.suggestionsProvider didSelectSuggestion:suggestion];
}

- (void)refocus {
  if (_needsRefocus && _webStateList) {
    [self incrementDismissCount];

    web::WebState* activeWebState = _webStateList->GetActiveWebState();
    password_manager::PasswordManagerJavaScriptFeature* feature =
        password_manager::PasswordManagerJavaScriptFeature::GetInstance();
    web::WebFrame* frame =
        feature->GetWebFramesManager(activeWebState)->GetFrameWithId(_frameId);
    BottomSheetTabHelper::FromWebState(activeWebState)
        ->DetachListenersAndRefocus(frame);
  }
}

// Disables future refocus requests.
- (void)disableRefocus {
  _needsRefocus = false;
}

- (void)loadFaviconAtIndexPath:(NSIndexPath*)indexPath
           faviconBlockHandler:(FaviconLoader::FaviconAttributesCompletionBlock)
                                   faviconLoadedBlock {
  CHECK(_faviconLoader);
  // Try loading the url's favicon.
  GURL URL(base::SysNSStringToUTF8([self descriptionAtRow:indexPath.row]));
  if (!URL.is_empty()) {
    _faviconLoader->FaviconForPageUrl(
        URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
        /*fallback_to_google_server=*/NO, faviconLoadedBlock);
  } else {
    faviconLoadedBlock([self defaultGlobeIconAttributes]);
  }
}

#pragma mark - WebStateListObserver

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

#pragma mark - SavedPasswordsPresenterObserver

- (void)savedPasswordsDidChange {
  std::vector<password_manager::CredentialUIEntry> savedCredentials =
      _savedPasswordsPresenter->GetSavedCredentials();
  if (!savedCredentials.empty()) {
    // TODO(crbug.com/1422362): Get the CredentialUIEntry associated to this
    // FormSuggestion. From the saved credentials, we will find the ones
    // associated with the current suggestions, store them and send that
    // information to the consumer.
    [self.consumer setSuggestions:self.suggestions];
  }
}

#pragma mark - Private

// Returns the display description at a given row in the table view.
- (NSString*)descriptionAtRow:(NSInteger)row {
  FormSuggestion* formSuggestion = [self.suggestions objectAtIndex:row];
  return formSuggestion.displayDescription;
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
    _prefService->SetInteger(
        prefs::kIosPasswordBottomSheetDismissCount,
        _prefService->GetInteger(prefs::kIosPasswordBottomSheetDismissCount) +
            1);
  }
}

@end
