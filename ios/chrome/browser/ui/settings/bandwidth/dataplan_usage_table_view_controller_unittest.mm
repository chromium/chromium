// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/bandwidth/dataplan_usage_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/bandwidth/dataplan_usage_table_view_controller+Testing.h"

#import <memory>

#import "base/files/file_path.h"
#import "base/memory/ref_counted.h"
#import "base/test/task_environment.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/pref_service.h"
#import "components/sync_preferences/pref_service_mock_factory.h"
#import "ios/chrome/browser/prerender/model/prerender_pref.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/base/l10n/l10n_util_mac.h"

using prerender_prefs::NetworkPredictionSetting;

namespace {

const char* kPrefName = "SettingPref";

class DataplanUsageTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    pref_service_ = CreateLocalState();
    CreateController();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    dataplanController_ = [[DataplanUsageTableViewController alloc]
        initWithPrefs:pref_service_.get()
          settingPref:kPrefName
                title:@"CollectionTitle"];
    return dataplanController_;
  }

  std::unique_ptr<PrefService> CreateLocalState() {
    scoped_refptr<PrefRegistrySimple> registry(new PrefRegistrySimple());
    registry->RegisterIntegerPref(
        kPrefName, static_cast<int>(NetworkPredictionSetting::kDisabled));

    sync_preferences::PrefServiceMockFactory factory;
    base::FilePath path("DataplanUsageTableViewControllerTest.pref");
    factory.SetUserPrefsFile(path,
                             task_environment_.GetMainThreadTaskRunner().get());
    return factory.Create(registry.get());
  }

  // Verifies that the cell at `item` in `section` has the given `accessory`
  // type.
  void CheckTextItemAccessoryType(UITableViewCellAccessoryType accessory_type,
                                  int section,
                                  int item) {
    TableViewDetailTextItem* cell = GetTableViewItem(section, item);
    EXPECT_EQ(accessory_type, cell.accessoryType);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<PrefService> pref_service_;
  DataplanUsageTableViewController* dataplanController_;
};

TEST_F(DataplanUsageTableViewControllerTest, TestModel) {
  CheckController();
  EXPECT_EQ(1, NumberOfSections());

  // No section header + 3 rows
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 1);
  CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, 2);

  CheckTextCellTextWithId(IDS_IOS_OPTIONS_DATA_USAGE_ALWAYS, 0, 0);
  CheckTextCellTextWithId(IDS_IOS_OPTIONS_DATA_USAGE_ONLY_WIFI, 0, 1);
  CheckTextCellTextWithId(IDS_IOS_OPTIONS_DATA_USAGE_NEVER, 0, 2);
}

TEST_F(DataplanUsageTableViewControllerTest, TestUpdateCheckedState) {
  CheckController();
  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(3, NumberOfItemsInSection(0));

  [dataplanController_
      updateSetting:NetworkPredictionSetting::kEnabledWifiOnly];
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, 1);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 2);

  [dataplanController_
      updateSetting:NetworkPredictionSetting::kEnabledWifiAndCellular];
  CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, 0);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 1);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 2);

  [dataplanController_ updateSetting:NetworkPredictionSetting::kDisabled];
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 1);
  CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, 2);
}

}  // namespace
