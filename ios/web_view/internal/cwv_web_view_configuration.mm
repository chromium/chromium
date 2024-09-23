// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "base/threading/thread_restrictions.h"
#import "components/affiliations/core/browser/affiliation_service.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/keyed_service/ios/browser_state_dependency_manager.h"
#import "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#import "components/password_manager/core/browser/password_store/password_store_interface.h"
#import "components/sync/service/sync_service.h"
#import "ios/web_view/internal/affiliations/web_view_affiliation_service_factory.h"
#import "ios/web_view/internal/app/application_context.h"
#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"
#import "ios/web_view/internal/autofill/web_view_personal_data_manager_factory.h"
#import "ios/web_view/internal/browser_state_keyed_service_factories.h"
#import "ios/web_view/internal/cwv_preferences_internal.h"
#import "ios/web_view/internal/cwv_user_content_controller_internal.h"
#import "ios/web_view/internal/cwv_web_view_configuration_internal.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#import "ios/web_view/internal/passwords/cwv_leak_check_service_internal.h"
#import "ios/web_view/internal/passwords/cwv_reuse_check_service_internal.h"
#import "ios/web_view/internal/passwords/web_view_account_password_store_factory.h"
#import "ios/web_view/internal/passwords/web_view_bulk_leak_check_service_factory.h"
#import "ios/web_view/internal/signin/web_view_identity_manager_factory.h"
#import "ios/web_view/internal/sync/cwv_sync_controller_internal.h"
#import "ios/web_view/internal/sync/web_view_sync_service_factory.h"
#import "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_global_state_util.h"

namespace {
CWVWebViewConfiguration* gDefaultConfiguration = nil;
CWVWebViewConfiguration* gIncognitoConfiguration = nil;
NSHashTable<CWVWebViewConfiguration*>* gNonPersistentConfigurations = nil;
}  // namespace

@interface CWVWebViewConfiguration () {
  // The BrowserState for this configuration.
  std::unique_ptr<ios_web_view::WebViewBrowserState> _browserState;

  // Holds all CWVWebViews created with this class. Weak references.
  NSHashTable* _webViews;
}

@end

@implementation CWVWebViewConfiguration

@synthesize autofillDataManager = _autofillDataManager;
@synthesize leakCheckService = _leakCheckService;
@synthesize reuseCheckService = _reuseCheckService;
@synthesize preferences = _preferences;
@synthesize syncController = _syncController;
@synthesize userContentController = _userContentController;

+ (void)initialize {
  if (self != [CWVWebViewConfiguration class]) {
    return;
  }

  ios_web_view::InitializeGlobalState();
  ios_web_view::EnsureBrowserStateKeyedServiceFactoriesBuilt();

  BrowserStateDependencyManager::GetInstance()
      ->DisallowKeyedServiceFactoryRegistration(
          "ios_web_view::EnsureBrowserStateKeyedServiceFactoriesBuilt()");
}

+ (void)shutDown {
  // Non-persistent configurations should be shut down first because its browser
  // state holds on to the default configuration's browser state. This ensures
  // the non-persistent configurations will not reference a dangling pointer.
  for (CWVWebViewConfiguration* nonPersistentConfiguration in
           gNonPersistentConfigurations) {
    [nonPersistentConfiguration shutDown];
  }
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
    gIncognitoConfiguration = [self nonPersistentConfiguration];
  });
  return gIncognitoConfiguration;
}

+ (CWVWebViewConfiguration*)nonPersistentConfiguration {
  CWVWebViewConfiguration* defaultConfiguration = [self defaultConfiguration];
  auto browserState = std::make_unique<ios_web_view::WebViewBrowserState>(
      /* off_the_record = */ true, defaultConfiguration.browserState);
  CWVWebViewConfiguration* nonPersistentConfiguration =
      [[CWVWebViewConfiguration alloc]
          initWithBrowserState:std::move(browserState)];

  // Save a weak pointer to nonpersistent configurations so they may be shut
  // down later.
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    gNonPersistentConfigurations = [NSHashTable weakObjectsHashTable];
  });
  [gNonPersistentConfigurations addObject:nonPersistentConfiguration];

  return nonPersistentConfiguration;
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
    scoped_refptr<password_manager::PasswordStoreInterface> passwordStore =
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
        ios_web_view::WebViewSyncServiceFactory::GetForBrowserState(
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

#pragma mark - LeakCheckService

- (CWVLeakCheckService*)leakCheckService {
  if (!_leakCheckService && self.persistent) {
    password_manager::BulkLeakCheckServiceInterface* bulkLeakCheckService =
        ios_web_view::WebViewBulkLeakCheckServiceFactory::GetForBrowserState(
            self.browserState);
    _leakCheckService = [[CWVLeakCheckService alloc]
        initWithBulkLeakCheckService:bulkLeakCheckService];
  }
  return _leakCheckService;
}

#pragma mark - ReuseCheckService

- (CWVReuseCheckService*)reuseCheckService {
  if (!_reuseCheckService && self.persistent) {
    affiliations::AffiliationService* affiliation_service =
        ios_web_view::WebViewAffiliationServiceFactory::GetForBrowserState(
            static_cast<ios_web_view::WebViewBrowserState*>(self.browserState));

    _reuseCheckService = [[CWVReuseCheckService alloc]
        initWithAffiliationService:affiliation_service];
  }
  return _reuseCheckService;
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
  [_autofillDataManager shutDown];
  for (CWVWebView* webView in _webViews) {
    [webView shutDown];
  }
  _browserState.reset();
}

@end
