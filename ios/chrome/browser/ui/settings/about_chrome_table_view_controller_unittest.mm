// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/about_chrome_table_view_controller.h"

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class AboutChromeTableViewControllerTest
    : public ChromeTableViewControllerTest {
 public:
  ChromeTableViewController* InstantiateController() override {
    return [[AboutChromeTableViewController alloc] init];
  }
};

TEST_F(AboutChromeTableViewControllerTest, TestModel) {
  CreateController();
  CheckController();

  EXPECT_EQ(1, NumberOfSections());
  EXPECT_EQ(3, NumberOfItemsInSection(0));
  EXPECT_NE(nil, [controller().tableViewModel footerForSection:0]);
  CheckTextCellTextWithId(IDS_IOS_OPEN_SOURCE_LICENSES, 0, 0);
  CheckTextCellTextWithId(IDS_IOS_TERMS_OF_SERVICE, 0, 1);
  CheckTextCellTextWithId(IDS_IOS_PRIVACY_POLICY, 0, 2);
}

}  // namespace
