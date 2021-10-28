// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/content_settings/core/common/features.h"
#include "components/handoff/pref_names_ios.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#import "components/sync/driver/mock_sync_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#include "ios/chrome/browser/sync/sync_service_factory.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ::testing::Return;

namespace {

NSString* const kSpdyProxyEnabled = @"SpdyProxyEnabled";

std::unique_ptr<KeyedService> BuildMockSyncService(web::BrowserState* context) {
  return std::make_unique<syncer::MockSyncService>();
}

class PrivacyTableViewControllerTest : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(CreatePrefService());
    test_cbs_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockSyncService));
    chrome_browser_state_ = test_cbs_builder.Build();

    browser_ = std::make_unique<TestBrowser>(chrome_browser_state_.get());

    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    initialValueForSpdyProxyEnabled_ =
        [[defaults valueForKey:kSpdyProxyEnabled] copy];
    [defaults setValue:@"Disabled" forKey:kSpdyProxyEnabled];
  }

  void TearDown() override {
    if (initialValueForSpdyProxyEnabled_) {
      [[NSUserDefaults standardUserDefaults]
          setObject:initialValueForSpdyProxyEnabled_
             forKey:kSpdyProxyEnabled];
    } else {
      [[NSUserDefaults standardUserDefaults]
          removeObjectForKey:kSpdyProxyEnabled];
    }
    ChromeTableViewControllerTest::TearDown();
  }

  // Makes a PrefService to be used by the test.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    RegisterBrowserStatePrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;
    return factory.CreateSyncable(registry.get());
  }

  ChromeTableViewController* InstantiateController() override {
    return [[PrivacyTableViewController alloc] initWithBrowser:browser_.get()
                                        reauthenticationModule:nil];
  }

  syncer::MockSyncService* mock_sync_service() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForBrowserState(chrome_browser_state_.get()));
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<Browser> browser_;
  NSString* initialValueForSpdyProxyEnabled_;
};

// Tests PrivacyTableViewController is set up with all appropriate items
// and sections.
TEST_F(PrivacyTableViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  EXPECT_EQ(3, NumberOfSections());

  // Sections[0].
  EXPECT_EQ(1, NumberOfItemsInSection(0));
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE), nil, 0, 0);

  // Sections[1].
  EXPECT_EQ(1, NumberOfItemsInSection(1));
  NSString* handoffSubtitle = chrome_browser_state_->GetPrefs()->GetBoolean(
                                  prefs::kIosHandoffToOtherDevices)
                                  ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  CheckTextCellTextAndDetailText(
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES),
      handoffSubtitle, 1, 0);

  // Sections[2].
  EXPECT_EQ(1, NumberOfItemsInSection(2));
  CheckSwitchCellStateAndText(
      NO, l10n_util::GetNSString(IDS_IOS_INCOGNITO_REAUTH_SETTING_NAME), 2, 0);

  CheckSectionFooter(
      l10n_util::GetNSString(IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER),
      /* section= */ 0);
}

// Tests PrivacyTableViewController sets the correct privacy footer for a
// non-syncing user.
TEST_F(PrivacyTableViewControllerTest, TestModelFooterWithSyncDisabled) {
  ON_CALL(*mock_sync_service()->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(false));

  CreateController();
  CheckController();
  EXPECT_EQ(3, NumberOfSections());

  CheckSectionFooter(
      l10n_util::GetNSString(IDS_IOS_PRIVACY_GOOGLE_SERVICES_FOOTER),
      /* section= */ 0);
}

// Tests PrivacyTableViewController sets the correct privacy footer for a
// syncing user.
TEST_F(PrivacyTableViewControllerTest, TestModelFooterWithSyncEnabled) {
  ON_CALL(*mock_sync_service()->GetMockUserSettings(), IsFirstSetupComplete())
      .WillByDefault(Return(true));
  ON_CALL(*mock_sync_service(), HasSyncConsent()).WillByDefault(Return(true));

  CreateController();
  CheckController();
  EXPECT_EQ(3, NumberOfSections());

  CheckSectionFooter(
      l10n_util::GetNSString(IDS_IOS_PRIVACY_SYNC_AND_GOOGLE_SERVICES_FOOTER),
      /* section= */ 0);
}

}  // namespace
