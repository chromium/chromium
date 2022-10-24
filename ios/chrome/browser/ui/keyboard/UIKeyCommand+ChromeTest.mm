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

void Verify(UIKeyCommand* command,
            NSString* symbolicDescription,
            NSString* action) {
  EXPECT_NSEQ(command.cr_symbolicDescription, symbolicDescription);
  EXPECT_TRUE(sel_isEqual(command.action, NSSelectorFromString(action)));
  EXPECT_EQ(command.title.length, 0u);
  EXPECT_EQ(command.discoverabilityTitle.length, 0u);
}

void Verify(UIKeyCommand* command,
            NSString* symbolicDescription,
            NSString* action,
            int messageID) {
  EXPECT_NSEQ(command.cr_symbolicDescription, symbolicDescription);
  EXPECT_TRUE(sel_isEqual(command.action, NSSelectorFromString(action)));
  EXPECT_NSEQ(command.title, l10n_util::GetNSStringWithFixup(messageID));
  EXPECT_NSEQ(command.discoverabilityTitle, command.title);
}

// Tests that UIKeyCommand-s are correctly created.
TEST_F(UIKeyCommandChromeTest, Factories) {
  Verify(UIKeyCommand.cr_openNewTab, @"⌘T", @"keyCommand_openNewTab",
         IDS_IOS_TOOLS_MENU_NEW_TAB);
  Verify(UIKeyCommand.cr_openNewTab_2, @"⌘N", @"keyCommand_openNewTab");
  // TODO(crbug.com/1376444): Verify all flavors of commands.
}

}  // namespace
