// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_consumer.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_empty_state_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_granular_fill_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_no_data_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_query_unsupported_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_recent_fills_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_results_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_view_controller.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using autofill::AtMemoryViewState;

using AtMemoryViewControllerTest = PlatformTest;

// Tests that setting all possible AtMemoryViewState values on the view
// controller transitions to the correct child view controller states.
TEST_F(AtMemoryViewControllerTest, TransitionsToCorrectChildViewControllers) {
  AtMemoryViewController* viewController =
      [[AtMemoryViewController alloc] init];
  // Trigger view load.
  (void)viewController.view;

  [viewController setViewState:AtMemoryViewState::kEmpty];
  EXPECT_EQ(viewController.childViewControllers.count, 1u);
  EXPECT_TRUE([viewController.childViewControllers.firstObject
      isKindOfClass:[AtMemoryEmptyStateViewController class]]);

  // TODO(crbug.com/522326512): Verify other states when they are implemented.
  [viewController setViewState:AtMemoryViewState::kRecentFills];
  EXPECT_EQ(viewController.childViewControllers.count, 1u);
  EXPECT_TRUE([viewController.childViewControllers.firstObject
      isKindOfClass:[AtMemoryRecentFillsViewController class]]);

  [viewController setViewState:AtMemoryViewState::kGranularFill];
  EXPECT_EQ(viewController.childViewControllers.count, 1u);
  EXPECT_TRUE([viewController.childViewControllers.firstObject
      isKindOfClass:[AtMemoryGranularFillViewController class]]);

  [viewController setViewState:AtMemoryViewState::kSearch];
  EXPECT_EQ(viewController.childViewControllers.count, 1u);
  EXPECT_TRUE([viewController.childViewControllers.firstObject
      isKindOfClass:[AtMemorySearchViewController class]]);

  [viewController setViewState:AtMemoryViewState::kSearchResults];
  EXPECT_EQ(viewController.childViewControllers.count, 1u);
  EXPECT_TRUE([viewController.childViewControllers.firstObject
      isKindOfClass:[AtMemorySearchResultsViewController class]]);

  [viewController setViewState:AtMemoryViewState::kQueryUnsupported];
  EXPECT_EQ(viewController.childViewControllers.count, 1u);
  EXPECT_TRUE([viewController.childViewControllers.firstObject
      isKindOfClass:[AtMemoryQueryUnsupportedViewController class]]);

  [viewController setViewState:AtMemoryViewState::kNoData];
  EXPECT_EQ(viewController.childViewControllers.count, 1u);
  EXPECT_TRUE([viewController.childViewControllers.firstObject
      isKindOfClass:[AtMemoryNoDataViewController class]]);
}

}  // namespace
