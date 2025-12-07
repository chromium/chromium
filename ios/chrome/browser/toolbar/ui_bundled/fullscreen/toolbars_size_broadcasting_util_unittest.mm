// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size_broadcasting_util.h"

#import "ios/chrome/browser/broadcaster/ui_bundled/chrome_broadcaster.h"
#import "ios/chrome/browser/toolbar/ui_bundled/fullscreen/toolbars_size.h"
#import "ios/chrome/browser/toolbar/ui_bundled/test/toolbars_size_broadcast_test_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using ToolbarsSizeBroadcastingUtilTest = PlatformTest;

// Tests that the ToolbarsSizeBroadcastingUtil functions successfully start
// and stop broadcasting toolbar properties.
TEST_F(ToolbarsSizeBroadcastingUtilTest, StartStop) {
  ToolbarsSize* toolbars_size =
      [[ToolbarsSize alloc] initWithCollapsedTopToolbarHeight:0.0
                                     expandedTopToolbarHeight:0.0
                                  expandedBottomToolbarHeight:0.0
                                 collapsedBottomToolbarHeight:0.0];
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  VerifyToolbarsSizeBroadcast(toolbars_size, broadcaster, false);
  StartBroadcastingToolbarsSize(toolbars_size, broadcaster);
  VerifyToolbarsSizeBroadcast(toolbars_size, broadcaster, true);
  StopBroadcastingToolbarsSize(broadcaster);
  VerifyToolbarsSizeBroadcast(toolbars_size, broadcaster, false);
}
