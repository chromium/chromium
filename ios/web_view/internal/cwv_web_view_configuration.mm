// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"

#include <memory>

#include "base/threading/thread_restrictions.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store_impl.h"
#include "components/sync/driver/sync_service.h"
#include "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"
#include "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#import "ios/web_view/internal/cwv_preferences_internal.h"
#import "ios/web_view/internal/cwv_user_content_controller_internal.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#include "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"
#import "ios/web_view/internal/sync/web_view_profile_sync_service_factory.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#include "ios/web_view/internal/web_view_global_state_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVWebViewConfiguration () {
  // The BrowserState for this configuration.
  std::unique_ptr<ios_web_view::WebViewBrowserState> _browserState;

  // Holds all CWVWebViews created with this class. Weak references.
  NSHashTable* _webViews;
}

@end

@implementation CWVWebViewConfiguration

@synthesize autofillDataManager = _autofillDataManager;
@synthesize preferences = _preferences;
@synthesize syncController = _syncController;
@synthesize userContentController = _userContentController;

namespace {
CWVWebViewConfiguration* gDefaultConfiguration = nil;
CWVWebViewConfiguration* gIncognitoConfiguration = nil;
}  // namespace

+ (void)shutDown {
  // Incognito should be shut down first because it holds onto members of the
  // non-incognito browser state. This ensures that the non-incognito browser
  // state will not leave any dangling references.
  [gIncognitoConfiguration shutDown];
  [gDefaultConfiguration shutDown];
}

+ (instancetype)defaultConfiguration {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    auto browserState = std::make_unique<ios_web_view::WebViewBrowserState>(
        /* off_the_record = */ false);
    gDefaultConfiguration = [[CWVWebViewConfiguration alloc]
        initWithBrowserState:std::move(browserState)];
  });
  return gDefaultConfiguration;
}

+ (instancetype)incognitoConfiguration {
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    CWVWebViewConfiguration* defaultConfiguration = [self defaultConfiguration];
    auto browserState = std::make_unique<ios_web_view::WebViewBrowserState>(
        /* off_the_record = */ true, defaultConfiguration.browserState);
    gIncognitoConfiguration = [[CWVWebViewConfiguration alloc]
        initWithBrowserState:std::move(browserState)];
  });
  return gIncognitoConfiguration;
}

+ (void)initialize {
  if (self != [CWVWebViewConfiguration class]) {
    return;
  }

  ios_web_view::InitializeGlobalState();
}

- (instancetype)initWithBrowserState:
    (std::unique_ptr<ios_web_view::WebViewBrowserState>)browserState {
  self = [super init];
  if (self) {
    _browserState = std::move(browserState);

    _preferences =
        [[CWVPreferences alloc] initWithPrefService:_browserState->GetPrefs()];

    _userContentController =
        [[CWVUserContentController alloc] initWithConfiguration:self];

    _webViews = [NSHashTable weakObjectsHashTable];
  }
  return self;
}

#pragma mark - Autofill

- (CWVAutofillDataManager*)autofillDataManager {
  if (!_autofillDataManager && self.persistent) {
    autofill::PersonalDataManager* personalDataManager =
        ios_web_view::WebViewPersonalDataManagerFactory::GetForBrowserState(
            self.browserState);
    scoped_refptr<password_manager::PasswordStore> passwordStore =
        ios_web_view::WebViewAccountPasswordStoreFactory::GetForBrowserState(
            self.browserState, ServiceAccessType::EXPLICIT_ACCESS);
    _autofillDataManager = [[CWVAutofillDataManager alloc]
        initWithPersonalDataManager:personalDataManager
                      passwordStore:passwordStore.get()];
  }
  return _autofillDataManager;
}

#pragma mark - Sync

- (CWVSyncController*)syncController {
  if (!_syncController && self.persistent) {
    syncer::SyncService* syncService =
        ios_web_view::WebViewProfileSyncServiceFactory::GetForBrowserState(
            self.browserState);
    signin::IdentityManager* identityManager =
        ios_web_view::WebViewIdentityManagerFactory::GetForBrowserState(
            self.browserState);
    _syncController = [[CWVSyncController alloc]
        initWithSyncService:syncService
            identityManager:identityManager
                prefService:_browserState->GetPrefs()];
  }
  return _syncController;
}

#pragma mark - Public Methods

- (BOOL)isPersistent {
  return !_browserState->IsOffTheRecord();
}

#pragma mark - Private Methods

- (ios_web_view::WebViewBrowserState*)browserState {
  return _browserState.get();
}

- (void)registerWebView:(CWVWebView*)webView {
  [_webViews addObject:webView];
}

- (void)shutDown {
  for (CWVWebView* webView in _webViews) {
    [webView shutDown];
  }
  _browserState.reset();
}

@end
