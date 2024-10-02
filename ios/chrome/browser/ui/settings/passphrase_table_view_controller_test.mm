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
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/fake_authentication_service_delegate.h"
#import "ios/chrome/browser/signin/model/fake_system_identity.h"
#import "ios/chrome/browser/signin/model/fake_system_identity_manager.h"
#import "ios/chrome/browser/sync/model/mock_sync_service_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

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
  RegisterProfilePrefs(registry.get());
  return prefs;
}

std::unique_ptr<KeyedService> CreateNiceMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<NiceMock<syncer::MockSyncService>>();
}

}  // anonymous namespace

PassphraseTableViewControllerTest::PassphraseTableViewControllerTest()
    : LegacyChromeTableViewControllerTest(),
      fake_sync_service_(nullptr),
      default_auth_error_(GoogleServiceAuthError::NONE) {}

PassphraseTableViewControllerTest::~PassphraseTableViewControllerTest() {}

void PassphraseTableViewControllerTest::SetUp() {
  LegacyChromeTableViewControllerTest::SetUp();

  // Set up the default return values for non-trivial return types.
  DefaultValue<const GoogleServiceAuthError&>::Set(default_auth_error_);
  DefaultValue<syncer::SyncCycleSnapshot>::Set(default_sync_cycle_snapshot_);

  TestProfileIOS::Builder builder;
  builder.AddTestingFactory(AuthenticationServiceFactory::GetInstance(),
                            AuthenticationServiceFactory::GetDefaultFactory());
  builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                            base::BindRepeating(&CreateNiceMockSyncService));
  RegisterTestingFactories(builder);
  builder.SetPrefService(CreatePrefService());
  profile_ = std::move(builder).Build();
  AuthenticationServiceFactory::CreateAndInitializeForProfile(
      profile_.get(), std::make_unique<FakeAuthenticationServiceDelegate>());
  app_state_ = [[AppState alloc] initWithStartupInformation:nil];
  scene_state_ = [[SceneState alloc] initWithAppState:app_state_];
  browser_ = std::make_unique<TestBrowser>(profile_.get(), scene_state_);

  fake_sync_service_ = static_cast<syncer::MockSyncService*>(
      SyncServiceFactory::GetForProfile(profile_.get()));

  // Set up non-default return values for our sync service mock.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*fake_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));

  FakeSystemIdentityManager* fake_system_identity_manager =
      FakeSystemIdentityManager::FromSystemIdentityManager(
          GetApplicationContext()->GetSystemIdentityManager());
  FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
  fake_system_identity_manager->AddIdentity(fake_identity);

  ChromeAccountManagerService* account_manager_service =
      ChromeAccountManagerServiceFactory::GetForProfile(profile_.get());
  AuthenticationService* auth_service =
      AuthenticationServiceFactory::GetForProfile(profile_.get());
  auth_service->SignIn(account_manager_service->GetDefaultIdentity(),
                       signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN);
}

void PassphraseTableViewControllerTest::TearDown() {
  // If the navigation controller exists, clear any of its child view
  // controllers.
  [nav_controller_ cleanUpSettings];
  nav_controller_ = nil;
  LegacyChromeTableViewControllerTest::TearDown();
}

void PassphraseTableViewControllerTest::RegisterTestingFactories(
    TestProfileIOS::Builder& builder) {
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
