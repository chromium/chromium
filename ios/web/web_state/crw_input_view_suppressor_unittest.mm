// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/crw_input_view_suppressor.h"

#import <UIKit/UIKit.h>

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

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

}  // namespace
