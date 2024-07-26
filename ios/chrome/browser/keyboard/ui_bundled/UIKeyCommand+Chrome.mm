// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"

#import "ios/chrome/browser/keyboard/ui_bundled/key_command_actions.h"

namespace {

UIKeyModifierFlags None = 0;
UIKeyModifierFlags Command = UIKeyModifierCommand;
UIKeyModifierFlags Control = UIKeyModifierControl;
UIKeyModifierFlags AltCommand = UIKeyModifierAlternate | UIKeyModifierCommand;
UIKeyModifierFlags ShiftCommand = UIKeyModifierShift | UIKeyModifierCommand;
UIKeyModifierFlags AltShiftCommand =
    UIKeyModifierAlternate | UIKeyModifierShift | UIKeyModifierCommand;
UIKeyModifierFlags ControlShift = UIKeyModifierControl | UIKeyModifierShift;

}  // namespace

@implementation UIKeyCommand (Chrome)

#pragma mark - Specific Keyboard Commands

+ (UIKeyCommand*)cr_openNewTab {
  return [self cr_commandWithInput:@"t"
                     modifierFlags:Command
                            action:@selector(keyCommand_openNewTab)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_NEW_TAB"];
}

+ (UIKeyCommand*)cr_openNewRegularTab {
  return [self keyCommandWithInput:@"n"
                     modifierFlags:Command
                            action:@selector(keyCommand_openNewRegularTab)];
}

+ (UIKeyCommand*)cr_openNewIncognitoTab {
  return [self cr_commandWithInput:@"n"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_openNewIncognitoTab)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_NEW_INCOGNITO_TAB"];
}

+ (UIKeyCommand*)cr_openNewWindow {
  return [self cr_commandWithInput:@"n"
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_openNewWindow)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_NEW_WINDOW"];
}

+ (UIKeyCommand*)cr_openNewIncognitoWindow {
  return [self cr_commandWithInput:@"n"
                     modifierFlags:AltShiftCommand
                            action:@selector(keyCommand_openNewIncognitoWindow)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_NEW_INCOGNITO_WINDOW"];
}

+ (UIKeyCommand*)cr_reopenLastClosedTab {
  return [self cr_commandWithInput:@"t"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_reopenLastClosedTab)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_REOPEN_CLOSED_TAB"];
}

+ (UIKeyCommand*)cr_find {
  return [self cr_commandWithInput:@"f"
                     modifierFlags:Command
                            action:@selector(keyCommand_find)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_FIND"];
}

+ (UIKeyCommand*)cr_findNext {
  return [self cr_commandWithInput:@"g"
                     modifierFlags:Command
                            action:@selector(keyCommand_findNext)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_FIND_NEXT"];
}

+ (UIKeyCommand*)cr_findPrevious {
  return [self cr_commandWithInput:@"g"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_findPrevious)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_FIND_PREVIOUS"];
}

+ (UIKeyCommand*)cr_openLocation {
  return [self cr_commandWithInput:@"l"
                     modifierFlags:Command
                            action:@selector(keyCommand_openLocation)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_OPEN_LOCATION"];
}

+ (UIKeyCommand*)cr_closeTab {
  return [self cr_commandWithInput:@"w"
                     modifierFlags:Command
                            action:@selector(keyCommand_closeTab)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_CLOSE_TAB"];
}

+ (UIKeyCommand*)cr_showNextTab {
  UIKeyCommand* keyCommand =
      [self cr_commandWithInput:@"\t"
                  modifierFlags:Control
                         action:@selector(keyCommand_showNextTab)
                titleIDAsString:@"IDS_IOS_KEYBOARD_NEXT_TAB"];
  keyCommand.wantsPriorityOverSystemBehavior = YES;
  return keyCommand;
}

+ (UIKeyCommand*)cr_showPreviousTab {
  UIKeyCommand* keyCommand =
      [self cr_commandWithInput:@"\t"
                  modifierFlags:ControlShift
                         action:@selector(keyCommand_showPreviousTab)
                titleIDAsString:@"IDS_IOS_KEYBOARD_PREVIOUS_TAB"];
  keyCommand.wantsPriorityOverSystemBehavior = YES;
  return keyCommand;
}

+ (UIKeyCommand*)cr_showNextTab_2 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of braces.
  return [self keyCommandWithInput:@"}"
                     modifierFlags:Command
                            action:@selector(keyCommand_showNextTab)];
}

+ (UIKeyCommand*)cr_showPreviousTab_2 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of braces.
  return [self keyCommandWithInput:@"{"
                     modifierFlags:Command
                            action:@selector(keyCommand_showPreviousTab)];
}

+ (UIKeyCommand*)cr_showNextTab_3 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of arrows.
  return [self keyCommandWithInput:UIKeyInputRightArrow
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showNextTab)];
}

+ (UIKeyCommand*)cr_showPreviousTab_3 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of arrows.
  return [self keyCommandWithInput:UIKeyInputLeftArrow
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showPreviousTab)];
}

+ (UIKeyCommand*)cr_showBookmarks {
  return [self cr_commandWithInput:@"b"
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showBookmarks)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_SHOW_BOOKMARKS"];
}

+ (UIKeyCommand*)cr_addToBookmarks {
  return [self cr_commandWithInput:@"d"
                     modifierFlags:Command
                            action:@selector(keyCommand_addToBookmarks)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_ADD_TO_BOOKMARKS"];
}

+ (UIKeyCommand*)cr_reload {
  return [self cr_commandWithInput:@"r"
                     modifierFlags:Command
                            action:@selector(keyCommand_reload)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_RELOAD"];
}

+ (UIKeyCommand*)cr_back {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of brackets.
  return [self cr_commandWithInput:@"["
                     modifierFlags:Command
                            action:@selector(keyCommand_back)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_HISTORY_BACK"];
}

+ (UIKeyCommand*)cr_forward {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of brackets.
  return [self cr_commandWithInput:@"]"
                     modifierFlags:Command
                            action:@selector(keyCommand_forward)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_HISTORY_FORWARD"];
}

+ (UIKeyCommand*)cr_back_2 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is
  // true by default. It handles flipping the direction of arrows.
  return [self keyCommandWithInput:UIKeyInputLeftArrow
                     modifierFlags:Command
                            action:@selector(keyCommand_back)];
}

+ (UIKeyCommand*)cr_forward_2 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is
  // true by default. It handles flipping the direction of arrows.
  return [self keyCommandWithInput:UIKeyInputRightArrow
                     modifierFlags:Command
                            action:@selector(keyCommand_forward)];
}

+ (UIKeyCommand*)cr_showHistory {
  return [self cr_commandWithInput:@"y"
                     modifierFlags:Command
                            action:@selector(keyCommand_showHistory)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_SHOW_HISTORY"];
}

+ (UIKeyCommand*)cr_voiceSearch {
  return [self cr_commandWithInput:@"."
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_voiceSearch)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_VOICE_SEARCH"];
}

+ (UIKeyCommand*)cr_close {
  return [self keyCommandWithInput:UIKeyInputEscape
                     modifierFlags:None
                            action:@selector(keyCommand_close)];
}

+ (UIKeyCommand*)cr_showSettings {
  return [self cr_commandWithInput:@","
                     modifierFlags:Command
                            action:@selector(keyCommand_showSettings)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_SHOW_SETTINGS"];
}

+ (UIKeyCommand*)cr_stop {
  return [self cr_commandWithInput:@"."
                     modifierFlags:Command
                            action:@selector(keyCommand_stop)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_STOP"];
}

+ (UIKeyCommand*)cr_showHelp {
  return [self cr_commandWithInput:@"?"
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showHelp)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_SHOW_HELP"];
}

+ (UIKeyCommand*)cr_showDownloads {
  return [self cr_commandWithInput:@"l"
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showDownloads)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_SHOW_DOWNLOADS"];
}

+ (UIKeyCommand*)cr_showDownloads_2 {
  return [self keyCommandWithInput:@"j"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_showDownloads)];
}

+ (UIKeyCommand*)cr_select1 {
  return [self keyCommandWithInput:@"1"
                     modifierFlags:Command
                            action:@selector(keyCommand_select1)];
}

+ (UIKeyCommand*)cr_select2 {
  return [self keyCommandWithInput:@"2"
                     modifierFlags:Command
                            action:@selector(keyCommand_select2)];
}

+ (UIKeyCommand*)cr_select3 {
  return [self keyCommandWithInput:@"3"
                     modifierFlags:Command
                            action:@selector(keyCommand_select3)];
}

+ (UIKeyCommand*)cr_select4 {
  return [self keyCommandWithInput:@"4"
                     modifierFlags:Command
                            action:@selector(keyCommand_select4)];
}

+ (UIKeyCommand*)cr_select5 {
  return [self keyCommandWithInput:@"5"
                     modifierFlags:Command
                            action:@selector(keyCommand_select5)];
}

+ (UIKeyCommand*)cr_select6 {
  return [self keyCommandWithInput:@"6"
                     modifierFlags:Command
                            action:@selector(keyCommand_select6)];
}

+ (UIKeyCommand*)cr_select7 {
  return [self keyCommandWithInput:@"7"
                     modifierFlags:Command
                            action:@selector(keyCommand_select7)];
}

+ (UIKeyCommand*)cr_select8 {
  return [self keyCommandWithInput:@"8"
                     modifierFlags:Command
                            action:@selector(keyCommand_select8)];
}

+ (UIKeyCommand*)cr_select9 {
  return [self cr_commandWithInput:@"9"
                     modifierFlags:Command
                            action:@selector(keyCommand_select9)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_LAST_TAB"];
}

+ (UIKeyCommand*)cr_reportAnIssue {
  return [self cr_commandWithInput:@"i"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_reportAnIssue)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_REPORT_AN_ISSUE"];
}

+ (UIKeyCommand*)cr_reportAnIssue_2 {
  return [self keyCommandWithInput:@"i"
                     modifierFlags:AltShiftCommand
                            action:@selector(keyCommand_reportAnIssue)];
}

+ (UIKeyCommand*)cr_addToReadingList {
  return [self cr_commandWithInput:@"d"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_addToReadingList)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_ADD_TO_READING_LIST"];
}

+ (UIKeyCommand*)cr_showReadingList {
  return [self cr_commandWithInput:@"r"
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showReadingList)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_SHOW_READING_LIST"];
}

+ (UIKeyCommand*)cr_goToTabGrid {
  return [self cr_commandWithInput:@"\\"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_goToTabGrid)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_GO_TO_TAB_GRID"];
}

+ (UIKeyCommand*)cr_clearBrowsingData {
  return [self cr_commandWithInput:UIKeyInputDelete
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_clearBrowsingData)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_CLEAR_BROWSING_DATA"];
}

+ (UIKeyCommand*)cr_closeAll {
  return [self cr_commandWithInput:@"w"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_closeAll)
                   titleIDAsString:@"IDS_IOS_KEYBOARD_CLOSE_ALL"];
}

+ (UIKeyCommand*)cr_undo {
  UIKeyCommand* keyCommand =
      [self keyCommandWithInput:@"z"
                  modifierFlags:Command
                         action:@selector(keyCommand_undo)];
  keyCommand.wantsPriorityOverSystemBehavior = YES;
  return keyCommand;
}

#pragma mark - Symbolic Description

- (NSString*)cr_symbolicDescription {
  NSMutableString* description = [NSMutableString string];

  if (self.modifierFlags & UIKeyModifierNumericPad) {
    [description appendString:@"Num lock "];
  }
  if (self.modifierFlags & UIKeyModifierControl) {
    [description appendString:@"⌃"];
  }
  if (self.modifierFlags & UIKeyModifierAlternate) {
    [description appendString:@"⌥"];
  }
  if (self.modifierFlags & UIKeyModifierShift) {
    [description appendString:@"⇧"];
  }
  if (self.modifierFlags & UIKeyModifierAlphaShift) {
    [description appendString:@"⇪"];
  }
  if (self.modifierFlags & UIKeyModifierCommand) {
    [description appendString:@"⌘"];
  }

  if ([self.input isEqualToString:UIKeyInputDelete]) {
    [description appendString:@"⌫"];
  } else if ([self.input isEqualToString:@"\r"]) {
    [description appendString:@"↵"];
  } else if ([self.input isEqualToString:@"\t"]) {
    [description appendString:@"⇥"];
  } else if ([self.input isEqualToString:UIKeyInputUpArrow]) {
    [description appendString:@"↑"];
  } else if ([self.input isEqualToString:UIKeyInputDownArrow]) {
    [description appendString:@"↓"];
  } else if ([self.input isEqualToString:UIKeyInputLeftArrow]) {
    [description appendString:@"←"];
  } else if ([self.input isEqualToString:UIKeyInputRightArrow]) {
    [description appendString:@"→"];
  } else if ([self.input isEqualToString:UIKeyInputEscape]) {
    [description appendString:@"⎋"];
  } else if ([self.input isEqualToString:@" "]) {
    [description appendString:@"␣"];
  } else {
    [description appendString:[self.input uppercaseString]];
  }
  return description;
}

#pragma mark - Private

// The title ID string is used as a key to NSLocalizedString because key
// commands can be requested by the OS very early on, before the resource bundle
// of the localized strings is loaded.
+ (instancetype)cr_commandWithInput:(NSString*)input
                      modifierFlags:(UIKeyModifierFlags)modifierFlags
                             action:(SEL)action
                    titleIDAsString:(NSString*)messageID {
  UIKeyCommand* keyCommand =
      [self commandWithTitle:NSLocalizedString(messageID, @"")
                       image:nil
                      action:action
                       input:input
               modifierFlags:modifierFlags
                propertyList:nil];
  keyCommand.discoverabilityTitle = keyCommand.title;
  return keyCommand;
}

@end
