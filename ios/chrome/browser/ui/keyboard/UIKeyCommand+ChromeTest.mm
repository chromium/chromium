// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"

#import <objc/runtime.h>

#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using UIKeyCommandChromeTest = PlatformTest;

// Tests that UIKeyCommand-s are correctly created.
TEST_F(UIKeyCommandChromeTest, UIKeyCommandFactory) {
  UIKeyCommand* command =
      [UIKeyCommand cr_commandWithInput:@"t"
                          modifierFlags:KeyModifierCommand
                                 action:@selector(description)
                                titleID:IDS_IOS_TOOLS_MENU_NEW_TAB];

  EXPECT_NSEQ(command.input, @"t");
  EXPECT_EQ(command.modifierFlags, UIKeyModifierCommand);
  EXPECT_TRUE(sel_isEqual(command.action, @selector(description)));
  EXPECT_NSEQ(command.title,
              l10n_util::GetNSStringWithFixup(IDS_IOS_TOOLS_MENU_NEW_TAB));
  EXPECT_NSEQ(command.discoverabilityTitle,
              l10n_util::GetNSStringWithFixup(IDS_IOS_TOOLS_MENU_NEW_TAB));
}

}  // namespace
