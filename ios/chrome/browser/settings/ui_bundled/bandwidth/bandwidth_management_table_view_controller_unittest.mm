// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bandwidth/bandwidth_management_table_view_controller.h"

#import <memory>
#import <utility>

#import "base/apple/foundation_util.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/sync_preferences/testing_pref_service_syncable.h"
#import "ios/chrome/browser/prerender/model/prerender_pref.h"
#import "ios/chrome/browser/settings/ui_bundled/bandwidth/dataplan_usage_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_table_view_controller_constants.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

using prerender_prefs::NetworkPredictionSetting;

class BandwidthManagementTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
  }

  void TearDown() override {
    [base::apple::ObjCCastStrict<BandwidthManagementTableViewController>(
        controller()) settingsWillBeDismissed];
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

// Tests that the default unmanaged model shows the editable "Preload webpages"
// detail item and section footer.
TEST_F(BandwidthManagementTableViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  EXPECT_NSEQ(kBandwidthSettingsTableViewId,
              controller().tableView.accessibilityIdentifier);
  EXPECT_EQ(1, NumberOfSections());

  EXPECT_EQ(1, NumberOfItemsInSection(0));
  NSString* expected_title =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_PRELOAD_WEBPAGES);
  NSString* expected_subtitle = [DataplanUsageTableViewController
      currentLabelForPreference:profile_->GetPrefs()
                    settingPref:prefs::kNetworkPredictionSetting];
  CheckTextCellTextAndDetailText(expected_title, expected_subtitle, 0, 0);
  EXPECT_NE(nil, [controller().tableViewModel footerForSectionIndex:0]);
}

// Tests that when `prefs::kNetworkPredictionSetting` is managed by enterprise
// policy, the "Preload webpages" row renders a non-selectable
// `TableViewInfoButtonItem` with the corresponding status text and does not
// push `DataplanUsageTableViewController`.
TEST_F(BandwidthManagementTableViewControllerTest, TestManagedModel) {
  struct ManagedTestCase {
    NetworkPredictionSetting setting;
    int expected_status_text_id;
  };
  constexpr ManagedTestCase kTestCases[] = {
      {.setting = NetworkPredictionSetting::kDisabled,
       .expected_status_text_id = IDS_IOS_OPTIONS_DATA_USAGE_NEVER},
      {.setting = NetworkPredictionSetting::kEnabledWifiOnly,
       .expected_status_text_id = IDS_IOS_OPTIONS_DATA_USAGE_ONLY_WIFI},
      {.setting = NetworkPredictionSetting::kEnabledWifiAndCellular,
       .expected_status_text_id = IDS_IOS_OPTIONS_DATA_USAGE_ALWAYS},
  };

  NSIndexPath* index_path = [NSIndexPath indexPathForRow:0 inSection:0];
  bool first = true;
  for (const ManagedTestCase& test_case : kTestCases) {
    if (!first) {
      [base::apple::ObjCCastStrict<BandwidthManagementTableViewController>(
          controller()) settingsWillBeDismissed];
      ResetController();
    }
    first = false;
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kNetworkPredictionSetting,
        base::Value(std::to_underlying(test_case.setting)));

    CreateController();
    CheckController();
    ASSERT_EQ(1, NumberOfSections());
    ASSERT_EQ(1, NumberOfItemsInSection(0));

    CheckInfoButtonCellStatusWithIdAndTextWithId(
        test_case.expected_status_text_id, IDS_IOS_OPTIONS_PRELOAD_WEBPAGES, 0,
        0);

    TableViewInfoButtonItem* managed_item =
        base::apple::ObjCCastStrict<TableViewInfoButtonItem>(
            GetTableViewItem(0, 0));
    ASSERT_TRUE(managed_item);
    EXPECT_NSEQ(kSettingsManagedPreloadCellId,
                managed_item.accessibilityIdentifier);
    EXPECT_NSEQ(l10n_util::GetNSString(
                    IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT),
                managed_item.accessibilityHint);

    UINavigationController* navigation_controller =
        [[UINavigationController alloc]
            initWithRootViewController:controller()];
    EXPECT_EQ(nil, [controller() tableView:controller().tableView
                       willSelectRowAtIndexPath:index_path]);
    [controller() tableView:controller().tableView
        didSelectRowAtIndexPath:index_path];
    EXPECT_EQ(controller(), navigation_controller.topViewController);
    EXPECT_EQ(1u, navigation_controller.viewControllers.count);
  }
}

// Tests that a recommended policy value updates the displayed detail text while
// keeping the "Preload webpages" row editable.
TEST_F(BandwidthManagementTableViewControllerTest,
       TestRecommendedModelRemainsEditable) {
  profile_->GetTestingPrefService()->SetRecommendedPref(
      prefs::kNetworkPredictionSetting,
      base::Value(std::to_underlying(
          NetworkPredictionSetting::kEnabledWifiAndCellular)));

  CreateController();
  CheckController();
  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(1, NumberOfItemsInSection(0));

  TableViewDetailIconItem* detail_item =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(
          GetTableViewItem(0, 0));
  ASSERT_TRUE(detail_item);
  EXPECT_NSEQ(kSettingsPreloadCellId, detail_item.accessibilityIdentifier);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPTIONS_DATA_USAGE_ALWAYS),
              detail_item.detailText);
  CheckAccessoryType(UITableViewCellAccessoryDisclosureIndicator, 0, 0);
}

// Tests that selecting the unmanaged "Preload webpages" row deselects it and
// pushes `DataplanUsageTableViewController`.
TEST_F(BandwidthManagementTableViewControllerTest,
       TestSelectPreloadRowDeselectsAndPushesSubpage) {
  CreateController();
  CheckController();

  ScopedKeyWindow scoped_key_window;
  UINavigationController* navigation_controller =
      [[UINavigationController alloc] initWithRootViewController:controller()];
  scoped_key_window.Get().rootViewController = navigation_controller;

  NSIndexPath* index_path = [NSIndexPath indexPathForRow:0 inSection:0];
  [controller().tableView selectRowAtIndexPath:index_path
                                      animated:NO
                                scrollPosition:UITableViewScrollPositionNone];
  ASSERT_NSEQ(index_path, controller().tableView.indexPathForSelectedRow);
  [controller() tableView:controller().tableView
      didSelectRowAtIndexPath:index_path];
  EXPECT_EQ(nil, controller().tableView.indexPathForSelectedRow);
  {
    SCOPED_TRACE("waiting for DataplanUsageTableViewController push");
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        base::test::ios::kWaitForUIElementTimeout, ^bool {
          return [navigation_controller.topViewController
                     isKindOfClass:[DataplanUsageTableViewController class]] &&
                 navigation_controller.transitionCoordinator == nil;
        }));
  }

  scoped_key_window.Get().rootViewController = nil;
}

// Tests that dynamically applying a mandatory policy swaps the row to
// `TableViewInfoButtonItem`, updates `statusText` on managed value changes,
// and restores `TableViewDetailIconItem` when the mandatory policy is removed.
TEST_F(BandwidthManagementTableViewControllerTest,
       TestDynamicManagedTransition) {
  CreateController();
  CheckController();

  sync_preferences::TestingPrefServiceSyncable* pref_service =
      profile_->GetTestingPrefService();
  pref_service->SetManagedPref(
      prefs::kNetworkPredictionSetting,
      base::Value(std::to_underlying(NetworkPredictionSetting::kDisabled)));
  CheckInfoButtonCellStatusWithIdAndTextWithId(
      IDS_IOS_OPTIONS_DATA_USAGE_NEVER, IDS_IOS_OPTIONS_PRELOAD_WEBPAGES, 0, 0);

  pref_service->SetManagedPref(
      prefs::kNetworkPredictionSetting,
      base::Value(std::to_underlying(
          NetworkPredictionSetting::kEnabledWifiAndCellular)));
  CheckInfoButtonCellStatusWithIdAndTextWithId(
      IDS_IOS_OPTIONS_DATA_USAGE_ALWAYS, IDS_IOS_OPTIONS_PRELOAD_WEBPAGES, 0,
      0);

  pref_service->RemoveManagedPref(prefs::kNetworkPredictionSetting);
  TableViewDetailIconItem* detail_item =
      base::apple::ObjCCastStrict<TableViewDetailIconItem>(
          GetTableViewItem(0, 0));
  ASSERT_TRUE(detail_item);
  EXPECT_NSEQ(kSettingsPreloadCellId, detail_item.accessibilityIdentifier);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_OPTIONS_DATA_USAGE_ONLY_WIFI),
              detail_item.detailText);
}

}  // namespace
