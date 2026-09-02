// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_controller.h"

#import "ios/chrome/browser/intelligence/actor/ui/actuation_header_view.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_constants.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_consumer.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_view_data.h"
#import "ios/chrome/browser/intelligence/actor/ui/test/actor_ui_test_utils.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using intelligence::actor::FindViewByAccessibilityIdentifier;
using ActuationWorklogViewControllerTest = PlatformTest;

// Test toggling compact mode updates subview visibility.
TEST_F(ActuationWorklogViewControllerTest, ToggleCompact) {
  ActuationWorklogViewController* view_controller =
      [[ActuationWorklogViewController alloc] init];

  UIView* compact_view = FindViewByAccessibilityIdentifier(
      view_controller.view, kCompactWorklogAccessibilityIdentifier);
  UIView* full_scroll_view = FindViewByAccessibilityIdentifier(
      view_controller.view, kFullWorklogScrollViewAccessibilityIdentifier);

  // Expanded worklog.
  view_controller.compact = NO;
  EXPECT_FALSE(view_controller.isCompact);
  EXPECT_TRUE(compact_view.hidden);
  EXPECT_FALSE(full_scroll_view.hidden);

  // Compact worklog.
  view_controller.compact = YES;
  EXPECT_TRUE(view_controller.isCompact);
  EXPECT_FALSE(compact_view.hidden);
  EXPECT_TRUE(full_scroll_view.hidden);
}

// Test toggling actuation active updates container visibility.
TEST_F(ActuationWorklogViewControllerTest, SetActuationActive) {
  ActuationWorklogViewController* view_controller =
      [[ActuationWorklogViewController alloc] init];

  id<ActuationWorklogConsumer> consumer =
      static_cast<id<ActuationWorklogConsumer>>(view_controller);

  // Inactive by default.
  EXPECT_TRUE(view_controller.view.hidden);

  [consumer setActuationActive:YES];
  EXPECT_FALSE(view_controller.view.hidden);

  [consumer setActuationActive:NO];
  EXPECT_TRUE(view_controller.view.hidden);
}

}  // namespace
