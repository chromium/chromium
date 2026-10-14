// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_input_view_suppressor.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

// Helper to create a single-button `UIBarButtonItemGroup` with `title`.
UIBarButtonItemGroup* CreateBarButtonGroup(NSString* title) {
  UIBarButtonItem* button =
      [[UIBarButtonItem alloc] initWithTitle:title
                                       style:UIBarButtonItemStylePlain
                                      target:nil
                                      action:nil];
  return [[UIBarButtonItemGroup alloc] initWithBarButtonItems:@[ button ]
                                           representativeItem:nil];
}

using CRWInputViewSuppressorTest = PlatformTest;

// Tests that `CRWInputViewSuppressor` returns a zero-sized `inputView` and
// `nil` for input view controllers and accessory views.
TEST_F(CRWInputViewSuppressorTest, ReturnsSuppressedInputViews) {
  UIView* root_view = [[UIView alloc] initWithFrame:CGRectZero];
  CRWInputViewSuppressor* suppressor =
      [[CRWInputViewSuppressor alloc] initWithView:root_view];

  UIView* input_view = [suppressor inputView];
  ASSERT_NE(input_view, nil);
  EXPECT_TRUE(CGRectIsEmpty(input_view.frame));
  EXPECT_EQ([suppressor inputView], input_view);
  EXPECT_EQ([suppressor inputViewController], nil);
  EXPECT_EQ([suppressor inputAccessoryView], nil);
  EXPECT_EQ([suppressor inputAccessoryViewController], nil);
}

// Tests that `reloadInputViews` and `disconnect` call `reloadInputViews` on the
// active first responder in the view hierarchy.
TEST_F(CRWInputViewSuppressorTest, ReloadsFirstResponderInputViews) {
  UIView* root_view = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* container_view = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* subview = [[UIView alloc] initWithFrame:CGRectZero];
  id mock_subview = OCMPartialMock(subview);
  OCMStub([mock_subview isFirstResponder]).andReturn(YES);
  [root_view addSubview:container_view];
  [container_view addSubview:subview];

  CRWInputViewSuppressor* suppressor =
      [[CRWInputViewSuppressor alloc] initWithView:root_view];
  ASSERT_NE(suppressor, nil);
  OCMExpect([mock_subview reloadInputViews]);
  [suppressor reloadInputViews];
  EXPECT_OCMOCK_VERIFY(mock_subview);

  OCMExpect([mock_subview reloadInputViews]);
  [suppressor disconnect];
  EXPECT_OCMOCK_VERIFY(mock_subview);

  // After `disconnect`, subsequent calls should be no-ops even while `subview`
  // remains in `root_view`.
  OCMReject([mock_subview reloadInputViews]);
  [suppressor reloadInputViews];
  [suppressor disconnect];

  // Calling `reloadInputViews` or `disconnect` on a connected suppressor when
  // no first responder is in the hierarchy should not reload detached views.
  [subview removeFromSuperview];
  CRWInputViewSuppressor* detached_suppressor =
      [[CRWInputViewSuppressor alloc] initWithView:root_view];
  [detached_suppressor reloadInputViews];
  [detached_suppressor disconnect];
  EXPECT_OCMOCK_VERIFY(mock_subview);
  [mock_subview stopMocking];
}

// Tests that `CRWInputViewSuppressor` suppresses and restores
// `inputAssistantItem` groups only for the active first responder, leaving
// non-active responder subviews untouched.
TEST_F(CRWInputViewSuppressorTest,
       SuppressesAndRestoresActiveFirstResponderAssistantItem) {
  UIView* root_view = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* active_subview = [[UIView alloc] initWithFrame:CGRectZero];
  id mock_active_subview = OCMPartialMock(active_subview);
  OCMStub([mock_active_subview isFirstResponder]).andReturn(YES);
  UIView* inactive_subview = [[UIView alloc] initWithFrame:CGRectZero];
  [root_view addSubview:active_subview];
  [root_view addSubview:inactive_subview];

  UITextInputAssistantItem* active_item = active_subview.inputAssistantItem;
  UITextInputAssistantItem* inactive_item = inactive_subview.inputAssistantItem;

  NSArray<UIBarButtonItemGroup*>* leading =
      @[ CreateBarButtonGroup(@"Leading") ];
  NSArray<UIBarButtonItemGroup*>* trailing =
      @[ CreateBarButtonGroup(@"Trailing") ];
  NSArray<UIBarButtonItemGroup*>* inactive_groups =
      @[ CreateBarButtonGroup(@"Inactive") ];

  active_item.leadingBarButtonGroups = leading;
  active_item.trailingBarButtonGroups = trailing;
  inactive_item.leadingBarButtonGroups = inactive_groups;

  CRWInputViewSuppressor* suppressor =
      [[CRWInputViewSuppressor alloc] initWithView:root_view];
  [suppressor reloadInputViews];
  EXPECT_EQ(active_item.leadingBarButtonGroups.count, 0u);
  EXPECT_EQ(active_item.trailingBarButtonGroups.count, 0u);
  EXPECT_NSEQ(inactive_item.leadingBarButtonGroups, inactive_groups);

  [suppressor disconnect];
  EXPECT_NSEQ(active_item.leadingBarButtonGroups, leading);
  EXPECT_NSEQ(active_item.trailingBarButtonGroups, trailing);
  [mock_active_subview stopMocking];
}

// Tests that bar button groups populated on the active first responder while
// `CRWInputViewSuppressor` is active are suppressed when `inputView` is
// accessed and restored when the responder changes or suppressor disconnects.
TEST_F(CRWInputViewSuppressorTest,
       SuppressesLazilyPopulatedInputAssistantItems) {
  UIView* root_view = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* subview_1 = [[UIView alloc] initWithFrame:CGRectZero];
  id mock_subview_1 = OCMPartialMock(subview_1);
  OCMStub([mock_subview_1 isFirstResponder]).andReturn(YES);
  UIView* subview_2 = [[UIView alloc] initWithFrame:CGRectZero];
  [root_view addSubview:subview_1];
  [root_view addSubview:subview_2];

  UITextInputAssistantItem* item_1 = subview_1.inputAssistantItem;
  UITextInputAssistantItem* item_2 = subview_2.inputAssistantItem;
  item_1.leadingBarButtonGroups = @[ CreateBarButtonGroup(@"Initial") ];
  item_1.trailingBarButtonGroups = @[];
  item_2.leadingBarButtonGroups = @[];
  item_2.trailingBarButtonGroups = @[];

  CRWInputViewSuppressor* suppressor =
      [[CRWInputViewSuppressor alloc] initWithView:root_view];
  [suppressor reloadInputViews];
  EXPECT_EQ(item_1.leadingBarButtonGroups.count, 0u);

  // Lazily update `leadingBarButtonGroups` then `trailingBarButtonGroups` on
  // `item_1` across separate `inputView` calls.
  NSArray<UIBarButtonItemGroup*>* leading_1 =
      @[ CreateBarButtonGroup(@"Leading1") ];
  item_1.leadingBarButtonGroups = leading_1;
  EXPECT_NE([suppressor inputView], nil);
  EXPECT_EQ(item_1.leadingBarButtonGroups.count, 0u);

  NSArray<UIBarButtonItemGroup*>* trailing_1 =
      @[ CreateBarButtonGroup(@"Trailing1") ];
  item_1.trailingBarButtonGroups = trailing_1;
  EXPECT_NE([suppressor inputView], nil);
  EXPECT_EQ(item_1.leadingBarButtonGroups.count, 0u);
  EXPECT_EQ(item_1.trailingBarButtonGroups.count, 0u);

  // Switch first responder to `subview_2` while `item_2` still has empty
  // groups; `item_1` should be restored immediately.
  [mock_subview_1 stopMocking];
  id mock_subview_2 = OCMPartialMock(subview_2);
  OCMStub([mock_subview_2 isFirstResponder]).andReturn(YES);
  EXPECT_NE([suppressor inputView], nil);
  EXPECT_NSEQ(item_1.leadingBarButtonGroups, leading_1);
  EXPECT_NSEQ(item_1.trailingBarButtonGroups, trailing_1);

  // Lazily populate `item_2` and suppress.
  NSArray<UIBarButtonItemGroup*>* leading_2 =
      @[ CreateBarButtonGroup(@"Leading2") ];
  NSArray<UIBarButtonItemGroup*>* trailing_2 =
      @[ CreateBarButtonGroup(@"Trailing2") ];
  item_2.leadingBarButtonGroups = leading_2;
  item_2.trailingBarButtonGroups = trailing_2;
  EXPECT_NE([suppressor inputView], nil);
  EXPECT_EQ(item_2.leadingBarButtonGroups.count, 0u);
  EXPECT_EQ(item_2.trailingBarButtonGroups.count, 0u);

  [suppressor disconnect];
  EXPECT_NSEQ(item_2.leadingBarButtonGroups, leading_2);
  EXPECT_NSEQ(item_2.trailingBarButtonGroups, trailing_2);
  [mock_subview_2 stopMocking];
}

// Tests that `CRWInputViewSuppressor` restores saved `inputAssistantItem`
// groups even if the responder resigns or is removed from the view hierarchy
// while suppressed.
TEST_F(CRWInputViewSuppressorTest,
       RestoresInputAssistantItemsAfterSubviewChanges) {
  UIView* root_view = [[UIView alloc] initWithFrame:CGRectZero];
  UIView* subview = [[UIView alloc] initWithFrame:CGRectZero];
  id mock_subview = OCMPartialMock(subview);
  OCMStub([mock_subview isFirstResponder]).andReturn(YES);
  [root_view addSubview:subview];

  UITextInputAssistantItem* subview_item = subview.inputAssistantItem;
  NSArray<UIBarButtonItemGroup*>* leading =
      @[ CreateBarButtonGroup(@"Leading") ];
  NSArray<UIBarButtonItemGroup*>* trailing =
      @[ CreateBarButtonGroup(@"Trailing") ];
  subview_item.leadingBarButtonGroups = leading;
  subview_item.trailingBarButtonGroups = trailing;

  CRWInputViewSuppressor* suppressor =
      [[CRWInputViewSuppressor alloc] initWithView:root_view];
  [suppressor reloadInputViews];
  EXPECT_EQ(subview_item.leadingBarButtonGroups.count, 0u);
  EXPECT_EQ(subview_item.trailingBarButtonGroups.count, 0u);

  // Resign and detach `subview` before restoring.
  [mock_subview stopMocking];
  [subview removeFromSuperview];
  EXPECT_NE([suppressor inputView], nil);

  [suppressor disconnect];
  EXPECT_NSEQ(subview_item.leadingBarButtonGroups, leading);
  EXPECT_NSEQ(subview_item.trailingBarButtonGroups, trailing);
}

}  // namespace
