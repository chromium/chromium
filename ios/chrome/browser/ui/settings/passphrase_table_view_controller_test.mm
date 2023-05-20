// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/passphrase_table_view_controller_test.h"

#import <UIKit/UIKit.h>

#import <memory>

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/sync/test/mock_sync_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service_mock.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::DefaultValue;
using testing::NiceMock;
using testing::Return;

namespace {

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
  sync_preferences::PrefServiceMockFactory factory;
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable);
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
      factory.CreateSyncable(registry.get());
  RegisterBrowserStatePrefs(registry.get());
  return prefs;
}

std::unique_ptr<KeyedService> CreateNiceMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<NiceMock<syncer::MockSyncService>>();
}

}  // anonymous namespace

PassphraseTableViewControllerTest::PassphraseTableViewControllerTest()
    : ChromeTableViewControllerTest(),
      fake_sync_service_(NULL),
      default_auth_error_(GoogleServiceAuthError::NONE) {}

PassphraseTableViewControllerTest::~PassphraseTableViewControllerTest() {}

void PassphraseTableViewControllerTest::SetUp() {
  ChromeTableViewControllerTest::SetUp();

  // Set up the default return values for non-trivial return types.
  DefaultValue<const GoogleServiceAuthError&>::Set(default_auth_error_);
  DefaultValue<syncer::SyncCycleSnapshot>::Set(default_sync_cycle_snapshot_);

  TestChromeBrowserState::Builder test_cbs_builder;
  test_cbs_builder.AddTestingFactory(
      AuthenticationServiceFactory::GetInstance(),
      AuthenticationServiceFactory::GetDefaultFactory());
  test_cbs_builder.AddTestingFactory(
      SyncSetupServiceFactory::GetInstance(),
      base::BindRepeating(&SyncSetupServiceMock::CreateKeyedService));
  test_cbs_builder.AddTestingFactory(
      SyncServiceFactory::GetInstance(),
      base::BindRepeating(&CreateNiceMockSyncService));
  RegisterTestingFactories(test_cbs_builder);
  test_cbs_builder.SetPrefService(CreatePrefService());
  chrome_browser_state_ = test_cbs_builder.Build();
  AuthenticationServiceFactory::CreateAndInitializeForBrowserState(
      chrome_browser_state_.get(),
      std::make_unique<FakeAuthenticationServiceDelegate>());
  browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());
  app_state_ = [[AppState alloc] initWithStartupInformation:nil];
  scene_state_ = [[SceneState alloc] initWithAppState:app_state_];
  SceneStateBrowserAgent::CreateForBrowser(browser_.get(), scene_state_);

  fake_sync_service_ = static_cast<syncer::MockSyncService*>(
      SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get()));

  // Set up non-default return values for our sync service mock.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*fake_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));

  FakeSystemIdentityManager* system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  system_identity_manager->AddIdentities(@[ @"identity1" ]);

  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  auth_service->SignIn(account_manager_service->GetDefaultIdentity(),
                       signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
}

void PassphraseTableViewControllerTest::TearDown() {
  // If the navigation controller exists, clear any of its child view
  // controllers.
  [nav_controller_ setViewControllers:@[] animated:NO];
  nav_controller_ = nil;
  ChromeTableViewControllerTest::TearDown();
}

void PassphraseTableViewControllerTest::RegisterTestingFactories(
    TestChromeBrowserState::Builder& builder) {
  // nothing to do, this is for sub-classes to override
}

void PassphraseTableViewControllerTest::SetUpNavigationController(
    UIViewController* test_controller) {
  dummy_controller_ = [[UIViewController alloc] init];
  nav_controller_ = [[SettingsNavigationController alloc]
      initWithRootViewController:dummy_controller_
                         browser:browser_.get()
                        delegate:nil];
  [nav_controller_ pushViewController:test_controller animated:NO];
}
