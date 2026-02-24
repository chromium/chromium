// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/coordinator/quick_delete_other_data_mediator.h"

#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/coordinator/quick_delete_util.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/ui/quick_delete_other_data_consumer.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ui/base/l10n/l10n_util_mac.h"

using quick_delete_util::DefaultSearchEngineState;

@interface QuickDeleteOtherDataMediator () <
    IdentityManagerObserverBridgeDelegate,
    SearchEngineObserving>
@end

@implementation QuickDeleteOtherDataMediator {
  // Service used to get the user's sign-in status.
  raw_ptr<AuthenticationService> _authenticationService;
  // Service used to create an observer that tracks if the sign-in status
  // has changed.
  raw_ptr<signin::IdentityManager> _identityManager;
  // Whether the user is signed in.
  BOOL _isSignedIn;
  // Observer for `IdentityManager`.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserver;

  // Service used to retrieve the default search engine.
  raw_ptr<TemplateURLService> _templateURLService;
  // Provides the current default search engine state.
  quick_delete_util::DefaultSearchEngineState _defaultSearchEngineState;
  // Observer bridge for search engine changes.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
}

- (instancetype)
    initWithAuthenticationService:(AuthenticationService*)authenticationService
                  identityManager:(signin::IdentityManager*)identityManager
               templateURLService:(TemplateURLService*)templateURLService {
  if ((self = [super init])) {
    _authenticationService = authenticationService;
    _identityManager = identityManager;
    _isSignedIn = _authenticationService->HasPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    _identityManagerObserver =
        std::make_unique<signin::IdentityManagerObserverBridge>(
            _identityManager, self);
    _templateURLService = templateURLService;
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, templateURLService);
    _defaultSearchEngineState =
        quick_delete_util::GetDefaultSearchEngineState(_templateURLService);
  }
  return self;
}

- (void)setConsumer:(id<QuickDeleteOtherDataConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;

  [self updateConsumer];
}

- (void)disconnect {
  _identityManagerObserver.reset();
  _searchEngineObserver.reset();

  _authenticationService = nullptr;
  _identityManager = nullptr;
  _templateURLService = nullptr;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      _isSignedIn = YES;
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      _isSignedIn = NO;
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      return;
  }

  [_consumer setShouldShowMyActivityCell:_isSignedIn];
  [_consumer setShouldShowSearchHistoryCell:[self shouldShowSearchHistoryCell]];
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  _defaultSearchEngineState =
      quick_delete_util::GetDefaultSearchEngineState(_templateURLService);

  [self updateConsumer];
}

#pragma mark - Private

// Updates the consumer with the current state.
- (void)updateConsumer {
  [_consumer setOtherDataPageTitle:[self otherDataPageTitle]];
  [_consumer setSearchHistoryCellSubtitle:[self searchHistoryCellSubtitle]];
  [_consumer setShouldShowMyActivityCell:_isSignedIn];
  [_consumer setShouldShowSearchHistoryCell:[self shouldShowSearchHistoryCell]];
}

// Returns the title for the "Quick Delete Other Data" page. The title depends
// on the user's default search engine.
- (NSString*)otherDataPageTitle {
  switch (_defaultSearchEngineState) {
    case DefaultSearchEngineState::kGoogle:
      return l10n_util::GetNSString(IDS_SETTINGS_OTHER_GOOGLE_DATA_TITLE);
    case DefaultSearchEngineState::kNotGoogle:
    case DefaultSearchEngineState::kError:
      return l10n_util::GetNSString(IDS_SETTINGS_OTHER_DATA_TITLE);
  }
  NOTREACHED();
}

// Returns the subtitle for the "Search history" cell. The subtitle depends on
// the user's default search engine.
- (NSString*)searchHistoryCellSubtitle {
  switch (_defaultSearchEngineState) {
    case DefaultSearchEngineState::kGoogle:
      return l10n_util::GetNSString(IDS_SETTINGS_MANAGE_IN_YOUR_GOOGLE_ACCOUNT);
    case DefaultSearchEngineState::kNotGoogle: {
      const TemplateURL* defaultSearchEngine =
          _templateURLService->GetDefaultSearchProvider();
      // Check if the default search engine is a known, prepopulated engine.
      // Prepopulated engines have a prepopulate_id > 0.
      return (defaultSearchEngine && defaultSearchEngine->prepopulate_id() > 0)
                 ? l10n_util::GetNSStringF(
                       IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_PREPOPULATED_DSE,
                       defaultSearchEngine->short_name())
                 : l10n_util::GetNSString(
                       IDS_SETTINGS_CLEAR_NON_GOOGLE_SEARCH_HISTORY_NON_PREPOPULATED_DSE);
    }
    case DefaultSearchEngineState::kError:
      // The "Search history" cell is not shown in this case, so no subtitle is
      // needed.
      return nil;
  }
  NOTREACHED();
}

// Returns whether the "Search history" cell should be displayed based on the
// user's default search engine and sign-in status.
- (BOOL)shouldShowSearchHistoryCell {
  switch (_defaultSearchEngineState) {
    case DefaultSearchEngineState::kGoogle:
      return _isSignedIn;
    case DefaultSearchEngineState::kNotGoogle:
      return YES;
    case DefaultSearchEngineState::kError:
      return NO;
  }
  NOTREACHED();
}

@end
