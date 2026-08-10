// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"

#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class AtMemorySearchViewControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    view_controller_ = [[AtMemorySearchViewController alloc]
        initWithStyle:UITableViewStylePlain];
    [view_controller_ loadViewIfNeeded];
  }

  void TearDown() override {
    view_controller_ = nil;
    PlatformTest::TearDown();
  }

  AtMemorySearchViewController* view_controller_;
};

// Tests that the view controller, navigation items, search bar, and table view
// are initialized properly.
TEST_F(AtMemorySearchViewControllerTest, TestInitialization) {
  EXPECT_NE(view_controller_.navigationItem.searchController, nil);
  EXPECT_NE(view_controller_.navigationItem.rightBarButtonItem, nil);
  EXPECT_NE(view_controller_.tableView, nil);
}

namespace {

// Returns the expected accessibility identifier for the given error type.
NSString* ExpectedAccessibilityIdentifier(AtMemoryErrorType error_type) {
  switch (error_type) {
    case AtMemoryErrorType::kNoDataError:
      return kAtMemoryNoDataCellAccessibilityIdentifier;
    case AtMemoryErrorType::kNoConnectionError:
      return kAtMemoryNoConnectionCellAccessibilityIdentifier;
    case AtMemoryErrorType::kUnsupportedQueryError:
      return kAtMemoryUnsupportedQueryCellAccessibilityIdentifier;
  }
}

}  // namespace

// Parameters for AtMemorySearchViewControllerErrorTest.
struct AtMemoryErrorTestParam {
  const char* test_name;
  AtMemoryErrorType error_type;
  CGFloat expected_alpha;
  bool expected_user_interaction_enabled;
};

// Parameterized test fixture to verify that setting error types configures
// the table view cell opacity and interaction state as expected.
class AtMemorySearchViewControllerErrorTest
    : public AtMemorySearchViewControllerTest,
      public ::testing::WithParamInterface<AtMemoryErrorTestParam> {};

// Tests that setting each error type properly configures the cell alpha and
// user interaction state.
TEST_P(AtMemorySearchViewControllerErrorTest, TestErrorState) {
  const AtMemoryErrorTestParam& param = GetParam();
  [view_controller_ setErrorType:param.error_type];

  EXPECT_EQ(view_controller_.tableView.numberOfSections, 1);
  EXPECT_EQ([view_controller_.tableView numberOfRowsInSection:0], 1);

  UITableViewCell* cell = [view_controller_.tableView.dataSource
                  tableView:view_controller_.tableView
      cellForRowAtIndexPath:[NSIndexPath indexPathForRow:0 inSection:0]];
  ASSERT_NE(cell, nil);
  EXPECT_EQ(cell.contentView.alpha, param.expected_alpha);
  EXPECT_EQ(cell.userInteractionEnabled,
            param.expected_user_interaction_enabled);
  EXPECT_NSEQ(cell.accessibilityIdentifier,
              ExpectedAccessibilityIdentifier(param.error_type));
}

// Instantiates the test suite with various combinations of error types to
// ensure cell opacity and interaction states are correctly configured.
INSTANTIATE_TEST_SUITE_P(
    All,
    AtMemorySearchViewControllerErrorTest,
    ::testing::Values(
        AtMemoryErrorTestParam{
            .test_name = "NoDataError",
            .error_type = AtMemoryErrorType::kNoDataError,
            .expected_alpha = kDefaultCellAlpha,
            .expected_user_interaction_enabled = false,
        },
        AtMemoryErrorTestParam{
            .test_name = "NoConnectionError",
            .error_type = AtMemoryErrorType::kNoConnectionError,
            .expected_alpha = kDisabledCellAlpha,
            .expected_user_interaction_enabled = false,
        },
        AtMemoryErrorTestParam{
            .test_name = "UnsupportedQueryError",
            .error_type = AtMemoryErrorType::kUnsupportedQueryError,
            .expected_alpha = kDefaultCellAlpha,
            .expected_user_interaction_enabled = true,
        }),
    [](const ::testing::TestParamInfo<AtMemoryErrorTestParam>& info) {
      return info.param.test_name;
    });
