// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using UIKeyCommandChromeTest = PlatformTest;

// Tests that UIApplication correctly calls the keyboard command action block
// when invoked.
TEST_F(UIKeyCommandChromeTest, UIApplicationHandleKeyCommand_CallsBlock) {
  __block BOOL called = NO;
  UIKeyCommand* command =
      [UIKeyCommand cr_keyCommandWithInput:@""
                             modifierFlags:Cr_UIKeyModifierNone
                                     title:nil
                                    action:^{
                                      called = YES;
                                    }];
  [[UIApplication sharedApplication] cr_handleKeyCommand:command];
  EXPECT_TRUE(called);
}

}  // namespace
