// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/passphrase_table_view_controller_test.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/sync/driver/mock_sync_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/authentication_service_fake.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::DefaultValue;
using testing::NiceMock;
using testing::Return;

std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
  sync_preferences::PrefServiceMockFactory factory;
  scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
      new user_prefs::PrefRegistrySyncable);
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs =
      factory.CreateSyncable(registry.get());
  RegisterBrowserStatePrefs(registry.get());
  return prefs;
}

std::unique_ptr<KeyedService>
PassphraseTableViewControllerTest::CreateNiceMockSyncService(
    web::BrowserState* context) {
  return std::make_unique<NiceMock<syncer::MockSyncService>>();
}

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
      base::BindRepeating(
          &AuthenticationServiceFake::CreateAuthenticationService));
  test_cbs_builder.SetPrefService(CreatePrefService());
  chrome_browser_state_ = test_cbs_builder.Build();
  WebStateList* web_state_list = nullptr;
  browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get(),
                                           web_state_list);

  fake_sync_service_ = static_cast<syncer::MockSyncService*>(
      ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          chrome_browser_state_.get(),
          base::BindRepeating(&CreateNiceMockSyncService)));
  ON_CALL(*fake_sync_service_, GetRegisteredDataTypes())
      .WillByDefault(Return(syncer::ModelTypeSet()));

  // Set up non-default return values for our sync service mock.
  ON_CALL(*fake_sync_service_->GetMockUserSettings(), IsPassphraseRequired())
      .WillByDefault(Return(true));
  ON_CALL(*fake_sync_service_, GetTransportState())
      .WillByDefault(Return(syncer::SyncService::TransportState::ACTIVE));

  ios::FakeChromeIdentityService* identityService =
      ios::FakeChromeIdentityService::GetInstanceFromChromeProvider();
  identityService->AddIdentities(@[ @"identity1" ]);
  ChromeIdentity* identity =
      [identityService->GetAllIdentitiesSortedForDisplay() objectAtIndex:0];
  AuthenticationServiceFactory::GetForBrowserState(chrome_browser_state_.get())
      ->SignIn(identity);
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
