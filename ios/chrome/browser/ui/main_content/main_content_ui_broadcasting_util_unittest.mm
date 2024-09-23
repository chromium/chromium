// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main_content/main_content_ui_broadcasting_util.h"

#import "ios/chrome/browser/ui/broadcaster/chrome_broadcaster.h"
#import "ios/chrome/browser/ui/main_content/main_content_ui.h"
#import "ios/chrome/browser/ui/main_content/test/main_content_broadcast_test_util.h"
#import "ios/chrome/browser/ui/main_content/test/test_main_content_ui_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

using MainContentUIBroadcastingUtilTest = PlatformTest;

// Test implementation of MainContentUI.
@interface TestMainContentUI : NSObject<MainContentUI>
@property(nonatomic, readonly) TestMainContentUIState* mainContentUIState;
@end

@implementation TestMainContentUI
@synthesize mainContentUIState = _mainContentUIState;

- (instancetype)init {
  if ((self = [super init])) {
    _mainContentUIState = [[TestMainContentUIState alloc] init];
  }
  return self;
}

@end

// Tests that the MainContentUIBroadcastingUtil functions successfully start
// and stop broadcasting main content properties.
TEST_F(MainContentUIBroadcastingUtilTest, StartStop) {
  TestMainContentUI* ui = [[TestMainContentUI alloc] init];
  ChromeBroadcaster* broadcaster = [[ChromeBroadcaster alloc] init];
  VerifyMainContentUIBroadcast(ui.mainContentUIState, broadcaster, false);
  StartBroadcastingMainContentUI(ui, broadcaster);
  VerifyMainContentUIBroadcast(ui.mainContentUIState, broadcaster, true);
  StopBroadcastingMainContentUI(broadcaster);
  VerifyMainContentUIBroadcast(ui.mainContentUIState, broadcaster, false);
}
