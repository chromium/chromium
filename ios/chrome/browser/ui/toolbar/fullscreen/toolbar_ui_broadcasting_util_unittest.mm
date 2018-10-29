// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui_broadcasting_util.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_broadcast_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using ToolbarUIBroadcastingUtilTest = PlatformTest;

// Tests that the ToolbarUIBroadcastingUtil functions successfully start
// and stop broadcasting toolbar properties.
TEST_F(ToolbarUIBroadcastingUtilTest, StartStop) {
  ToolbarUIState* toolbar_ui = [[ToolbarUIState alloc] init];
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  VerifyToolbarUIBroadcast(toolbar_ui, broadcaster, false);
  StartBroadcastingToolbarUI(toolbar_ui, broadcaster);
  VerifyToolbarUIBroadcast(toolbar_ui, broadcaster, true);
  StopBroadcastingToolbarUI(broadcaster);
  VerifyToolbarUIBroadcast(toolbar_ui, broadcaster, false);
}
