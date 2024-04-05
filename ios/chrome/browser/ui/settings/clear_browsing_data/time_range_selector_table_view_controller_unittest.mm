// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/time_range_selector_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/time_range_selector_table_view_controller+Testing.h"

#import "base/files/file_path.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

const NSInteger kNumberOfItems = 5;

class TimeRangeSelectorTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 protected:
  TimeRangeSelectorTableViewControllerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    LegacyChromeTableViewControllerTest::SetUp();
    CreateController();
  }

  LegacyChromeTableViewController* InstantiateController() override {
    time_range_selector_controller_ =
        [[TimeRangeSelectorTableViewController alloc]
            initWithTimePeriod:browsing_data::TimePeriod::LAST_HOUR];
    return time_range_selector_controller_;
  }

  // Verifies that the cell at `item` in `section` has the given `accessory`
  // type.
  void CheckTextItemAccessoryType(UITableViewCellAccessoryType accessory_type,
                                  int section,
                                  int item) {
    TableViewItem* cell = GetTableViewItem(section, item);
    EXPECT_EQ(accessory_type, cell.accessoryType);
  }

  base::test::TaskEnvironment task_environment_;
  TimeRangeSelectorTableViewController* time_range_selector_controller_;
};

TEST_F(TimeRangeSelectorTableViewControllerTest, TestModel) {
  CheckController();
  EXPECT_EQ(1, NumberOfSections());

  // No section header + 5 rows
  EXPECT_EQ(kNumberOfItems, NumberOfItemsInSection(0));

  CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, 0);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 1);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 2);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 3);
  CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, 4);

  CheckTextCellTextWithId(
      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_HOUR, 0, 0);
  CheckTextCellTextWithId(
      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_DAY, 0, 1);
  CheckTextCellTextWithId(
      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_PAST_WEEK, 0, 2);
  CheckTextCellTextWithId(
      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_LAST_FOUR_WEEKS, 0, 3);
  CheckTextCellTextWithId(
      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_OPTION_BEGINNING_OF_TIME, 0, 4);
}

TEST_F(TimeRangeSelectorTableViewControllerTest, TestUpdateCheckedState) {
  CheckController();
  ASSERT_EQ(1, NumberOfSections());
  ASSERT_EQ(kNumberOfItems, NumberOfItemsInSection(0));

  for (NSInteger checkedItem = 0; checkedItem < kNumberOfItems; ++checkedItem) {
    [time_range_selector_controller_
        updateTimePeriod:static_cast<browsing_data::TimePeriod>(checkedItem)];
    for (NSInteger item = 0; item < kNumberOfItems; ++item) {
      if (item == checkedItem) {
        CheckTextItemAccessoryType(UITableViewCellAccessoryCheckmark, 0, item);
      } else {
        CheckTextItemAccessoryType(UITableViewCellAccessoryNone, 0, item);
      }
    }
  }
}

}  // namespace
