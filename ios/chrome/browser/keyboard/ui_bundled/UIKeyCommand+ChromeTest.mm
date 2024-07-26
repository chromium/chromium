// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"

#import <objc/runtime.h>

#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

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
            NSString* messageIDAsString) {
  EXPECT_NSEQ(command.cr_symbolicDescription, symbolicDescription);
  EXPECT_TRUE(sel_isEqual(command.action, NSSelectorFromString(action)));
  EXPECT_NSEQ(command.title, NSLocalizedString(messageIDAsString, @""));
  EXPECT_NSEQ(command.discoverabilityTitle, command.title);
}

// Returns a UIKeyCommand with the given input, no modifiers, and a no-op
// action.
UIKeyCommand* KeyCommand(NSString* input) {
  return [UIKeyCommand keyCommandWithInput:input
                             modifierFlags:0
                                    action:@selector(self)];
}

// Checks that UIKeyCommand-s are correctly created.
TEST_F(UIKeyCommandChromeTest, Factories) {
  Verify(UIKeyCommand.cr_openNewTab, @"⌘T", @"keyCommand_openNewTab",
         @"IDS_IOS_KEYBOARD_NEW_TAB");
  Verify(UIKeyCommand.cr_openNewRegularTab, @"⌘N",
         @"keyCommand_openNewRegularTab");
  Verify(UIKeyCommand.cr_openNewIncognitoTab, @"⇧⌘N",
         @"keyCommand_openNewIncognitoTab",
         @"IDS_IOS_KEYBOARD_NEW_INCOGNITO_TAB");
  Verify(UIKeyCommand.cr_openNewWindow, @"⌥⌘N", @"keyCommand_openNewWindow",
         @"IDS_IOS_KEYBOARD_NEW_WINDOW");
  Verify(UIKeyCommand.cr_openNewIncognitoWindow, @"⌥⇧⌘N",
         @"keyCommand_openNewIncognitoWindow",
         @"IDS_IOS_KEYBOARD_NEW_INCOGNITO_WINDOW");
  Verify(UIKeyCommand.cr_reopenLastClosedTab, @"⇧⌘T",
         @"keyCommand_reopenLastClosedTab",
         @"IDS_IOS_KEYBOARD_REOPEN_CLOSED_TAB");
  Verify(UIKeyCommand.cr_find, @"⌘F", @"keyCommand_find",
         @"IDS_IOS_KEYBOARD_FIND");
  Verify(UIKeyCommand.cr_findNext, @"⌘G", @"keyCommand_findNext",
         @"IDS_IOS_KEYBOARD_FIND_NEXT");
  Verify(UIKeyCommand.cr_findPrevious, @"⇧⌘G", @"keyCommand_findPrevious",
         @"IDS_IOS_KEYBOARD_FIND_PREVIOUS");
  Verify(UIKeyCommand.cr_openLocation, @"⌘L", @"keyCommand_openLocation",
         @"IDS_IOS_KEYBOARD_OPEN_LOCATION");
  Verify(UIKeyCommand.cr_closeTab, @"⌘W", @"keyCommand_closeTab",
         @"IDS_IOS_KEYBOARD_CLOSE_TAB");
  Verify(UIKeyCommand.cr_showNextTab, @"⌃⇥", @"keyCommand_showNextTab",
         @"IDS_IOS_KEYBOARD_NEXT_TAB");
  Verify(UIKeyCommand.cr_showPreviousTab, @"⌃⇧⇥", @"keyCommand_showPreviousTab",
         @"IDS_IOS_KEYBOARD_PREVIOUS_TAB");
  Verify(UIKeyCommand.cr_showNextTab_2, @"⌘}", @"keyCommand_showNextTab");
  Verify(UIKeyCommand.cr_showPreviousTab_2, @"⌘{",
         @"keyCommand_showPreviousTab");
  Verify(UIKeyCommand.cr_showNextTab_3, @"⌥⌘→", @"keyCommand_showNextTab");
  Verify(UIKeyCommand.cr_showPreviousTab_3, @"⌥⌘←",
         @"keyCommand_showPreviousTab");
  Verify(UIKeyCommand.cr_showBookmarks, @"⌥⌘B", @"keyCommand_showBookmarks",
         @"IDS_IOS_KEYBOARD_SHOW_BOOKMARKS");
  Verify(UIKeyCommand.cr_addToBookmarks, @"⌘D", @"keyCommand_addToBookmarks",
         @"IDS_IOS_KEYBOARD_ADD_TO_BOOKMARKS");
  Verify(UIKeyCommand.cr_reload, @"⌘R", @"keyCommand_reload",
         @"IDS_IOS_KEYBOARD_RELOAD");
  Verify(UIKeyCommand.cr_back, @"⌘[", @"keyCommand_back",
         @"IDS_IOS_KEYBOARD_HISTORY_BACK");
  Verify(UIKeyCommand.cr_forward, @"⌘]", @"keyCommand_forward",
         @"IDS_IOS_KEYBOARD_HISTORY_FORWARD");
  Verify(UIKeyCommand.cr_back_2, @"⌘←", @"keyCommand_back");
  Verify(UIKeyCommand.cr_forward_2, @"⌘→", @"keyCommand_forward");
  Verify(UIKeyCommand.cr_showHistory, @"⌘Y", @"keyCommand_showHistory",
         @"IDS_IOS_KEYBOARD_SHOW_HISTORY");
  Verify(UIKeyCommand.cr_voiceSearch, @"⇧⌘.", @"keyCommand_voiceSearch",
         @"IDS_IOS_KEYBOARD_VOICE_SEARCH");
  Verify(UIKeyCommand.cr_close, @"⎋", @"keyCommand_close");
  Verify(UIKeyCommand.cr_showSettings, @"⌘,", @"keyCommand_showSettings",
         @"IDS_IOS_KEYBOARD_SHOW_SETTINGS");
  Verify(UIKeyCommand.cr_stop, @"⌘.", @"keyCommand_stop",
         @"IDS_IOS_KEYBOARD_STOP");
  Verify(UIKeyCommand.cr_showHelp, @"⌥⌘?", @"keyCommand_showHelp",
         @"IDS_IOS_KEYBOARD_SHOW_HELP");
  Verify(UIKeyCommand.cr_showDownloads, @"⌥⌘L", @"keyCommand_showDownloads",
         @"IDS_IOS_KEYBOARD_SHOW_DOWNLOADS");
  Verify(UIKeyCommand.cr_showDownloads_2, @"⇧⌘J", @"keyCommand_showDownloads");
  Verify(UIKeyCommand.cr_select1, @"⌘1", @"keyCommand_select1");
  Verify(UIKeyCommand.cr_select2, @"⌘2", @"keyCommand_select2");
  Verify(UIKeyCommand.cr_select3, @"⌘3", @"keyCommand_select3");
  Verify(UIKeyCommand.cr_select4, @"⌘4", @"keyCommand_select4");
  Verify(UIKeyCommand.cr_select5, @"⌘5", @"keyCommand_select5");
  Verify(UIKeyCommand.cr_select6, @"⌘6", @"keyCommand_select6");
  Verify(UIKeyCommand.cr_select7, @"⌘7", @"keyCommand_select7");
  Verify(UIKeyCommand.cr_select8, @"⌘8", @"keyCommand_select8");
  Verify(UIKeyCommand.cr_select9, @"⌘9", @"keyCommand_select9",
         @"IDS_IOS_KEYBOARD_LAST_TAB");
  Verify(UIKeyCommand.cr_reportAnIssue, @"⇧⌘I", @"keyCommand_reportAnIssue",
         @"IDS_IOS_KEYBOARD_REPORT_AN_ISSUE");
  Verify(UIKeyCommand.cr_reportAnIssue_2, @"⌥⇧⌘I", @"keyCommand_reportAnIssue");
  Verify(UIKeyCommand.cr_addToReadingList, @"⇧⌘D",
         @"keyCommand_addToReadingList",
         @"IDS_IOS_KEYBOARD_ADD_TO_READING_LIST");
  Verify(UIKeyCommand.cr_showReadingList, @"⌥⌘R", @"keyCommand_showReadingList",
         @"IDS_IOS_KEYBOARD_SHOW_READING_LIST");
  Verify(UIKeyCommand.cr_goToTabGrid, @"⇧⌘\\", @"keyCommand_goToTabGrid",
         @"IDS_IOS_KEYBOARD_GO_TO_TAB_GRID");
  Verify(UIKeyCommand.cr_clearBrowsingData, @"⇧⌘⌫",
         @"keyCommand_clearBrowsingData",
         @"IDS_IOS_KEYBOARD_CLEAR_BROWSING_DATA");
  Verify(UIKeyCommand.cr_closeAll, @"⇧⌘W", @"keyCommand_closeAll",
         @"IDS_IOS_KEYBOARD_CLOSE_ALL");
  Verify(UIKeyCommand.cr_undo, @"⌘Z", @"keyCommand_undo");
}

// Checks that modifiers in the symbolic description are correct (correct symbol
// and correct order).
TEST_F(UIKeyCommandChromeTest, SymbolicDescription_Modifiers) {
  UIKeyCommand* fullModifiers = [UIKeyCommand
      keyCommandWithInput:@"a"
            modifierFlags:UIKeyModifierNumericPad | UIKeyModifierControl |
                          UIKeyModifierAlternate | UIKeyModifierShift |
                          UIKeyModifierAlphaShift | UIKeyModifierCommand
                   action:@selector(self)];

  EXPECT_NSEQ(@"Num lock ⌃⌥⇧⇪⌘A", fullModifiers.cr_symbolicDescription);
}

// Checks that inputs in the symbolic description are correct (correct
// capitalization and symbolization).
TEST_F(UIKeyCommandChromeTest, SymbolicDescription_Inputs) {
  EXPECT_NSEQ(@"A", KeyCommand(@"a").cr_symbolicDescription);
  EXPECT_NSEQ(@"⌫", KeyCommand(@"\b").cr_symbolicDescription);
  EXPECT_NSEQ(@"↵", KeyCommand(@"\r").cr_symbolicDescription);
  EXPECT_NSEQ(@"⇥", KeyCommand(@"\t").cr_symbolicDescription);
  EXPECT_NSEQ(@"↑", KeyCommand(@"UIKeyInputUpArrow").cr_symbolicDescription);
  EXPECT_NSEQ(@"↓", KeyCommand(@"UIKeyInputDownArrow").cr_symbolicDescription);
  EXPECT_NSEQ(@"←", KeyCommand(@"UIKeyInputLeftArrow").cr_symbolicDescription);
  EXPECT_NSEQ(@"→", KeyCommand(@"UIKeyInputRightArrow").cr_symbolicDescription);
  EXPECT_NSEQ(@"⎋", KeyCommand(@"UIKeyInputEscape").cr_symbolicDescription);
  EXPECT_NSEQ(@"␣", KeyCommand(@" ").cr_symbolicDescription);
}

}  // namespace
