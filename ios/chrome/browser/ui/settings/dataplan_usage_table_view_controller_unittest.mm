// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/dataplan_usage_table_view_controller.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface DataplanUsageTableViewController (ExposedForTesting)
- (void)updateBasePref:(BOOL)basePref wifiPref:(BOOL)wifiPref;
@end

namespace {

const char* kBasePref = "BasePref";
const char* kWifiPref = "WifiPref";

class DataplanUsageTableViewControllerTest
    : public ChromeTableViewControllerTest {
 protected:
  void SetUp() override {
    ChromeTableViewControllerTest::SetUp();
    pref_service_ = CreateLocalState();
    CreateController();
  }

  ChromeTableViewController* InstantiateController() override {
    dataplanController_ = [[DataplanUsageTableViewController alloc]
        initWithPrefs:pref_service_.get()
             basePref:kBasePref
             wifiPref:kWifiPref
                title:@"CollectionTitle"];
    return dataplanController_;
  }

  std::unique_ptr<PrefService> CreateLocalState() {
    scoped_refptr<PrefRegistrySimple> registry(new PrefRegistrySimple());
    registry->RegisterBooleanPref(kBasePref, false);
    registry->RegisterBooleanPref(kWifiPref, false);

    sync_preferences::PrefServiceMockFactory factory;
    base::FilePath path("DataplanUsageTableViewControllerTest.pref");
    factory.SetUserPrefsFile(path,
                             task_environment_.GetMainThreadTaskRunner().get());
    return factory.Create(registry.get());
  }

  // Verifies that the cell at |item| in |section| has the given |accessory|
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

  [dataplanController_ updateBasePref:YES wifiPref:YES];
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, 1);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 2);

  [dataplanController_ updateBasePref:YES wifiPref:NO];
  CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, 0);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 1);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 2);

  [dataplanController_ updateBasePref:NO wifiPref:YES];
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 1);
  CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, 2);

  [dataplanController_ updateBasePref:NO wifiPref:NO];
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 0);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 1);
  CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, 2);
}

}  // namespace
