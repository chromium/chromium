// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui_broadcasting_util.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui.h"
#import "ios/chrome/browser/ui/toolbar/test/toolbar_broadcast_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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
