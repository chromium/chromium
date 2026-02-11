// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/quick_delete_other_data/coordinator/quick_delete_other_data_mediator.h"

#import "components/search_engines/template_url_service.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/settings/ui_bundled/clear_browsing_data/coordinator/quick_delete_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"

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

- (void)disconnect {
  _searchEngineObserver.reset();
  _templateURLService = nullptr;
  _identityManagerObserver.reset();
  _authenticationService = nullptr;
  _identityManager = nullptr;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  // TODO(crbug.com/464551859) Implementation of this method to be added after
  // addition of the consumer.
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  _defaultSearchEngineState =
      quick_delete_util::GetDefaultSearchEngineState(_templateURLService);
  // TODO(crbug.com/464551859) Implementation of this method to be added after
  // addition of the consumer.
}

@end
