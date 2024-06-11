// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/alert_view/ui_bundled/alert_view_controller.h"

#import "ios/chrome/browser/alert_view/ui_bundled/alert_action.h"
#import "testing/platform_test.h"

using AlertViewControllerTest = PlatformTest;

// Tests AlertViewController can be initiliazed.
TEST_F(AlertViewControllerTest, Init) {
  AlertViewController* alert = [[AlertViewController alloc] init];
  EXPECT_TRUE(alert);
}

// Tests there are no circular references in a simple init.
TEST_F(AlertViewControllerTest, Dealloc) {
  __weak AlertViewController* weakAlert = nil;
  @autoreleasepool {
    AlertViewController* alert = [[AlertViewController alloc] init];
    weakAlert = alert;
  }
  EXPECT_FALSE(weakAlert);
}

// Tests there are no circular references in an alert with actions.
TEST_F(AlertViewControllerTest, DeallocWithActions) {
  __weak AlertViewController* weakAlert = nil;
  @autoreleasepool {
    AlertViewController* alert = [[AlertViewController alloc] init];
    AlertAction* action =
        [AlertAction actionWithTitle:@"OK"
                               style:UIAlertActionStyleDefault
                             handler:^(AlertAction* alert_action){
                             }];
    [alert setActions:@[ @[ action ] ]];
    weakAlert = alert;
  }
  EXPECT_FALSE(weakAlert);
}
