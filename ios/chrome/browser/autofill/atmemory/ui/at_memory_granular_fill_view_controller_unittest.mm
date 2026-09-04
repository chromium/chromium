// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_item.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_mutator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kTestAttributeName = @"Passport Number";
NSString* const kTestAttributeValue = @"AB123456";
constexpr NSInteger kTestIndex = 0;

}  // namespace

using AtMemoryGranularFillViewControllerTest = PlatformTest;

// Tests that setting granular fill items updates the table view sections and
// rows.
TEST_F(AtMemoryGranularFillViewControllerTest, TestSetGranularFillItems) {
  AtMemoryGranularFillViewController* viewController =
      [[AtMemoryGranularFillViewController alloc]
          initWithStyle:ChromeTableViewStyle()];
  [viewController loadViewIfNeeded];

  AtMemoryGranularFillItem* item = [[AtMemoryGranularFillItem alloc]
      initWithAttributeName:kTestAttributeName
             attributeValue:kTestAttributeValue
                      index:kTestIndex];

  [viewController setGranularFillItems:@[ item ]];

  EXPECT_EQ(2, viewController.tableView.numberOfSections);
  EXPECT_EQ(1, [viewController.tableView numberOfRowsInSection:0]);
  EXPECT_EQ(1, [viewController.tableView numberOfRowsInSection:1]);
}
