// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"

#import <objc/runtime.h>

#import "base/i18n/rtl.h"
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
  Verify(UIKeyCommand.cr_openNewRegularTab, @"⌘N",
         @"keyCommand_openNewRegularTab");
  Verify(UIKeyCommand.cr_openNewIncognitoTab, @"⇧⌘N",
         @"keyCommand_openNewIncognitoTab",
         IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB);
  Verify(UIKeyCommand.cr_reopenLastClosedTab, @"⇧⌘T",
         @"keyCommand_reopenLastClosedTab", IDS_IOS_KEYBOARD_REOPEN_CLOSED_TAB);
  Verify(UIKeyCommand.cr_openFindInPage, @"⌘F", @"keyCommand_openFindInPage",
         IDS_IOS_TOOLS_MENU_FIND_IN_PAGE);
  Verify(UIKeyCommand.cr_findNextStringInPage, @"⌘G",
         @"keyCommand_findNextStringInPage");
  Verify(UIKeyCommand.cr_findPreviousStringInPage, @"⇧⌘G",
         @"keyCommand_findPreviousStringInPage");
  Verify(UIKeyCommand.cr_focusOmnibox, @"⌘L", @"keyCommand_focusOmnibox",
         IDS_IOS_KEYBOARD_OPEN_LOCATION);
  Verify(UIKeyCommand.cr_closeTab, @"⌘W", @"keyCommand_closeTab",
         IDS_IOS_TOOLS_MENU_CLOSE_TAB);
  Verify(UIKeyCommand.cr_showNextTab, @"⌃⇥", @"keyCommand_showNextTab",
         IDS_IOS_KEYBOARD_NEXT_TAB);
  Verify(UIKeyCommand.cr_showPreviousTab, @"⌃⇧⇥", @"keyCommand_showPreviousTab",
         IDS_IOS_KEYBOARD_PREVIOUS_TAB);
  Verify(UIKeyCommand.cr_showNextTab_2, @"⌘}", @"keyCommand_showNextTab");
  Verify(UIKeyCommand.cr_showPreviousTab_2, @"⌘{",
         @"keyCommand_showPreviousTab");
  Verify(UIKeyCommand.cr_showNextTab_3, @"⌥⌘→", @"keyCommand_showNextTab");
  Verify(UIKeyCommand.cr_showPreviousTab_3, @"⌥⌘←",
         @"keyCommand_showPreviousTab");
  Verify(UIKeyCommand.cr_showBookmarks, @"⌥⌘B", @"keyCommand_showBookmarks",
         IDS_IOS_KEYBOARD_SHOW_BOOKMARKS);
  Verify(UIKeyCommand.cr_addToBookmarks, @"⌘D", @"keyCommand_addToBookmarks",
         IDS_IOS_KEYBOARD_ADD_TO_BOOKMARKS);
  Verify(UIKeyCommand.cr_reload, @"⌘R", @"keyCommand_reload",
         IDS_IOS_ACCNAME_RELOAD);
  Verify(UIKeyCommand.cr_goBack, @"⌘[", @"keyCommand_goBack",
         IDS_IOS_KEYBOARD_HISTORY_BACK);
  Verify(UIKeyCommand.cr_goForward, @"⌘]", @"keyCommand_goForward",
         IDS_IOS_KEYBOARD_HISTORY_FORWARD);
  Verify(UIKeyCommand.cr_goBack_2, @"⌘←", @"keyCommand_goBack");
  Verify(UIKeyCommand.cr_goForward_2, @"⌘→", @"keyCommand_goForward");
  Verify(UIKeyCommand.cr_showHistory, @"⌘Y", @"keyCommand_showHistory",
         IDS_IOS_KEYBOARD_SHOW_HISTORY);
  Verify(UIKeyCommand.cr_startVoiceSearch, @"⇧⌘.",
         @"keyCommand_startVoiceSearch",
         IDS_IOS_VOICE_SEARCH_KEYBOARD_DISCOVERY_TITLE);
  Verify(UIKeyCommand.cr_close, @"⎋", @"keyCommand_close");
  Verify(UIKeyCommand.cr_showSettings, @"⌘,", @"keyCommand_showSettings");
  Verify(UIKeyCommand.cr_stop, @"⌘.", @"keyCommand_stop");
  Verify(UIKeyCommand.cr_showHelp, @"⌥⌘?", @"keyCommand_showHelp");
  Verify(UIKeyCommand.cr_showDownloadsFolder, @"⇧⌘J",
         @"keyCommand_showDownloadsFolder");
  Verify(UIKeyCommand.cr_showDownloadsFolder_2, @"⌥⌘L",
         @"keyCommand_showDownloadsFolder");
  Verify(UIKeyCommand.cr_showTab0, @"⌘1", @"keyCommand_showTab0");
  Verify(UIKeyCommand.cr_showTab1, @"⌘2", @"keyCommand_showTab1");
  Verify(UIKeyCommand.cr_showTab2, @"⌘3", @"keyCommand_showTab2");
  Verify(UIKeyCommand.cr_showTab3, @"⌘4", @"keyCommand_showTab3");
  Verify(UIKeyCommand.cr_showTab4, @"⌘5", @"keyCommand_showTab4");
  Verify(UIKeyCommand.cr_showTab5, @"⌘6", @"keyCommand_showTab5");
  Verify(UIKeyCommand.cr_showTab6, @"⌘7", @"keyCommand_showTab6");
  Verify(UIKeyCommand.cr_showTab7, @"⌘8", @"keyCommand_showTab7");
  Verify(UIKeyCommand.cr_showLastTab, @"⌘9", @"keyCommand_showLastTab");

  // Prior to iOS 15, RTL needs to be handled manually. Check it for key
  // commands that need to adapt.
  if (@available(iOS 15.0, *)) {
    // Nothing to do on iOS 15+.
  } else {
    base::i18n::SetRTLForTesting(true);
    Verify(UIKeyCommand.cr_showNextTab_2, @"⌘{", @"keyCommand_showNextTab");
    Verify(UIKeyCommand.cr_showPreviousTab_2, @"⌘}",
           @"keyCommand_showPreviousTab");
    Verify(UIKeyCommand.cr_showNextTab_3, @"⌥⌘←", @"keyCommand_showNextTab");
    Verify(UIKeyCommand.cr_showPreviousTab_3, @"⌥⌘→",
           @"keyCommand_showPreviousTab");
    Verify(UIKeyCommand.cr_goBack, @"⌘]", @"keyCommand_goBack",
           IDS_IOS_KEYBOARD_HISTORY_BACK);
    Verify(UIKeyCommand.cr_goForward, @"⌘[", @"keyCommand_goForward",
           IDS_IOS_KEYBOARD_HISTORY_FORWARD);
    Verify(UIKeyCommand.cr_goBack_2, @"⌘→", @"keyCommand_goBack");
    Verify(UIKeyCommand.cr_goForward_2, @"⌘←", @"keyCommand_goForward");
  }
}

}  // namespace
