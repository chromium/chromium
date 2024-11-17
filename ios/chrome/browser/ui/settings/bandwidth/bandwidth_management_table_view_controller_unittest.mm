// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/bandwidth/bandwidth_management_table_view_controller.h"

#import <memory>

#import "base/run_loop.h"
#import "base/test/test_simple_task_runner.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/testing_pref_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "components/sync_preferences/pref_service_syncable.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/ui/settings/bandwidth/dataplan_usage_table_view_controller.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

class BandwidthManagementTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 public:
  BandwidthManagementTableViewControllerTest() {}

 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();

    sync_preferences::PrefServiceMockFactory factory;
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs(
        factory.CreateSyncable(registry.get()));
    RegisterProfilePrefs(registry.get());
    TestProfileIOS::Builder builder;
    builder.SetPrefService(std::move(prefs));
    profile_ = std::move(builder).Build();

    CreateController();
  }

  void TearDown() override {
    base::RunLoop().RunUntilIdle();
    LegacyChromeTableViewControllerTest::TearDown();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    return [[BandwidthManagementTableViewController alloc]
        initWithProfile:profile_.get()];
  }

  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;

  std::unique_ptr<TestProfileIOS> profile_;
};

TEST_F(BandwidthManagementTableViewControllerTest, TestModel) {
  CheckController();
  EXPECT_EQ(1, NumberOfSections());

  EXPECT_EQ(1, NumberOfItemsInSection(0));
  // Preload webpages item.
  NSString* expected_title =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
  NSString* expected_subtitle = [DataplanUsageTableViewController
      currentLabelForPreference:profile_->GetPrefs()
                    settingPref:prefs::kNetworkPredictionSetting];
  CheckTextCellTextAndDetailText(expected_title, expected_subtitle, 0, 0);
  EXPECT_NE(nil, [controller().tableViewModel footerForSectionIndex:0]);
}

}  // namespace
