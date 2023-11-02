// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/about_chrome_table_view_controller.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"

namespace {

class AboutChromeTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 public:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[AboutChromeTableViewController alloc] init];
  }
};

TEST_F(AboutChromeTableViewControllerTest, TestModel) {
  CreateController();
  CheckController();

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  EXPECT_NE(nil, [controller().tableViewModel footerForSectionIndex:0]);
  CheckTextCellTextWithId(IDS_IOS_OPEN_SOURCE_LICENSES, 0, 0);
  CheckTextCellTextWithId(IDS_IOS_TERMS_OF_SERVICE, 0, 1);
  CheckTextCellTextWithId(IDS_IOS_PRIVACY_POLICY, 0, 2);
}

}  // namespace
