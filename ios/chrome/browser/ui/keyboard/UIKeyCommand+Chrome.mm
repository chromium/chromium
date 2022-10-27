// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIKeyModifierFlags None = 0;
UIKeyModifierFlags Command = UIKeyModifierCommand;
UIKeyModifierFlags Control = UIKeyModifierControl;
UIKeyModifierFlags AltCommand = UIKeyModifierAlternate | UIKeyModifierCommand;
UIKeyModifierFlags ShiftCommand = UIKeyModifierShift | UIKeyModifierCommand;
UIKeyModifierFlags AltShiftCommand =
    UIKeyModifierAlternate | UIKeyModifierShift | UIKeyModifierCommand;
UIKeyModifierFlags ControlShift = UIKeyModifierControl | UIKeyModifierShift;

@implementation UIKeyCommand (Chrome)

#pragma mark - Specific Keyboard Commands

+ (UIKeyCommand*)cr_openNewTab {
  return [self cr_commandWithInput:@"t"
                     modifierFlags:Command
                            action:@selector(keyCommand_openNewTab)
                           titleID:IDS_IOS_TOOLS_MENU_NEW_TAB];
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
                           titleID:IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB];
}

+ (UIKeyCommand*)cr_reopenLastClosedTab {
  return [self cr_commandWithInput:@"t"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_reopenLastClosedTab)
                           titleID:IDS_IOS_KEYBOARD_REOPEN_CLOSED_TAB];
}

+ (UIKeyCommand*)cr_openFindInPage {
  return [self cr_commandWithInput:@"f"
                     modifierFlags:Command
                            action:@selector(keyCommand_openFindInPage)
                           titleID:IDS_IOS_TOOLS_MENU_FIND_IN_PAGE];
}

+ (UIKeyCommand*)cr_findNextStringInPage {
  return [self keyCommandWithInput:@"g"
                     modifierFlags:Command
                            action:@selector(keyCommand_findNextStringInPage)];
}

+ (UIKeyCommand*)cr_findPreviousStringInPage {
  return
      [self keyCommandWithInput:@"g"
                  modifierFlags:ShiftCommand
                         action:@selector(keyCommand_findPreviousStringInPage)];
}

+ (UIKeyCommand*)cr_focusOmnibox {
  return [self cr_commandWithInput:@"l"
                     modifierFlags:Command
                            action:@selector(keyCommand_focusOmnibox)
                           titleID:IDS_IOS_KEYBOARD_OPEN_LOCATION];
}

+ (UIKeyCommand*)cr_closeTab {
  return [self cr_commandWithInput:@"w"
                     modifierFlags:Command
                            action:@selector(keyCommand_closeTab)
                           titleID:IDS_IOS_TOOLS_MENU_CLOSE_TAB];
}

+ (UIKeyCommand*)cr_showNextTab {
  UIKeyCommand* keyCommand =
      [self cr_commandWithInput:@"\t"
                  modifierFlags:Control
                         action:@selector(keyCommand_showNextTab)
                        titleID:IDS_IOS_KEYBOARD_NEXT_TAB];
  if (@available(iOS 15.0, *)) {
    keyCommand.wantsPriorityOverSystemBehavior = YES;
  }
  return keyCommand;
}

+ (UIKeyCommand*)cr_showPreviousTab {
  UIKeyCommand* keyCommand =
      [self cr_commandWithInput:@"\t"
                  modifierFlags:ControlShift
                         action:@selector(keyCommand_showPreviousTab)
                        titleID:IDS_IOS_KEYBOARD_PREVIOUS_TAB];
  if (@available(iOS 15.0, *)) {
    keyCommand.wantsPriorityOverSystemBehavior = YES;
  }
  return keyCommand;
}

+ (UIKeyCommand*)cr_showNextTab_2 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of braces.
  NSString* braceNext;
  if (@available(iOS 15.0, *)) {
    braceNext = @"}";
  } else {
    if (UseRTLLayout()) {
      braceNext = @"{";
    } else {
      braceNext = @"}";
    }
  }
  return [self keyCommandWithInput:braceNext
                     modifierFlags:Command
                            action:@selector(keyCommand_showNextTab)];
}

+ (UIKeyCommand*)cr_showPreviousTab_2 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of braces.
  NSString* bracePrevious;
  if (@available(iOS 15.0, *)) {
    bracePrevious = @"{";
  } else {
    if (UseRTLLayout()) {
      bracePrevious = @"}";
    } else {
      bracePrevious = @"{";
    }
  }
  return [self keyCommandWithInput:bracePrevious
                     modifierFlags:Command
                            action:@selector(keyCommand_showPreviousTab)];
}

+ (UIKeyCommand*)cr_showNextTab_3 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of arrows.
  NSString* arrowNext;
  if (@available(iOS 15.0, *)) {
    arrowNext = UIKeyInputRightArrow;
  } else {
    if (UseRTLLayout()) {
      arrowNext = UIKeyInputLeftArrow;
    } else {
      arrowNext = UIKeyInputRightArrow;
    }
  }
  return [self keyCommandWithInput:arrowNext
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showNextTab)];
}

+ (UIKeyCommand*)cr_showPreviousTab_3 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of arrows.
  NSString* arrowPrevious;
  if (@available(iOS 15.0, *)) {
    arrowPrevious = UIKeyInputLeftArrow;
  } else {
    if (UseRTLLayout()) {
      arrowPrevious = UIKeyInputRightArrow;
    } else {
      arrowPrevious = UIKeyInputLeftArrow;
    }
  }
  return [self keyCommandWithInput:arrowPrevious
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showPreviousTab)];
}

+ (UIKeyCommand*)cr_showBookmarks {
  return [self cr_commandWithInput:@"b"
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showBookmarks)
                           titleID:IDS_IOS_KEYBOARD_SHOW_BOOKMARKS];
}

+ (UIKeyCommand*)cr_addToBookmarks {
  return [self cr_commandWithInput:@"d"
                     modifierFlags:Command
                            action:@selector(keyCommand_addToBookmarks)
                           titleID:IDS_IOS_KEYBOARD_ADD_TO_BOOKMARKS];
}

+ (UIKeyCommand*)cr_reload {
  return [self cr_commandWithInput:@"r"
                     modifierFlags:Command
                            action:@selector(keyCommand_reload)
                           titleID:IDS_IOS_ACCNAME_RELOAD];
}

+ (UIKeyCommand*)cr_goBack {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of brackets.
  NSString* bracketBack;
  if (@available(iOS 15.0, *)) {
    bracketBack = @"[";
  } else {
    if (UseRTLLayout()) {
      bracketBack = @"]";
    } else {
      bracketBack = @"[";
    }
  }
  return [self cr_commandWithInput:bracketBack
                     modifierFlags:Command
                            action:@selector(keyCommand_goBack)
                           titleID:IDS_IOS_KEYBOARD_HISTORY_BACK];
}

+ (UIKeyCommand*)cr_goForward {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is true
  // by default. It handles flipping the direction of brackets.
  NSString* bracketForward;
  if (@available(iOS 15.0, *)) {
    bracketForward = @"]";
  } else {
    if (UseRTLLayout()) {
      bracketForward = @"[";
    } else {
      bracketForward = @"]";
    }
  }
  return [self cr_commandWithInput:bracketForward
                     modifierFlags:Command
                            action:@selector(keyCommand_goForward)
                           titleID:IDS_IOS_KEYBOARD_HISTORY_FORWARD];
}

+ (UIKeyCommand*)cr_goBack_2 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is
  // true by default. It handles flipping the direction of arrows.
  NSString* arrowBack;
  if (@available(iOS 15.0, *)) {
    arrowBack = UIKeyInputLeftArrow;
  } else {
    if (UseRTLLayout()) {
      arrowBack = UIKeyInputRightArrow;
    } else {
      arrowBack = UIKeyInputLeftArrow;
    }
  }
  return [self keyCommandWithInput:arrowBack
                     modifierFlags:Command
                            action:@selector(keyCommand_goBack)];
}

+ (UIKeyCommand*)cr_goForward_2 {
  // iOS 15+ supports -[UIKeyCommand allowsAutomaticMirroring], which is
  // true by default. It handles flipping the direction of arrows.
  NSString* arrowForward;
  if (@available(iOS 15.0, *)) {
    arrowForward = UIKeyInputRightArrow;
  } else {
    if (UseRTLLayout()) {
      arrowForward = UIKeyInputLeftArrow;
    } else {
      arrowForward = UIKeyInputRightArrow;
    }
  }
  return [self keyCommandWithInput:arrowForward
                     modifierFlags:Command
                            action:@selector(keyCommand_goForward)];
}

+ (UIKeyCommand*)cr_showHistory {
  return [self cr_commandWithInput:@"y"
                     modifierFlags:Command
                            action:@selector(keyCommand_showHistory)
                           titleID:IDS_IOS_KEYBOARD_SHOW_HISTORY];
}

+ (UIKeyCommand*)cr_startVoiceSearch {
  return
      [self cr_commandWithInput:@"."
                  modifierFlags:ShiftCommand
                         action:@selector(keyCommand_startVoiceSearch)
                        titleID:IDS_IOS_VOICE_SEARCH_KEYBOARD_DISCOVERY_TITLE];
}

+ (UIKeyCommand*)cr_close {
  return [self keyCommandWithInput:UIKeyInputEscape
                     modifierFlags:None
                            action:@selector(keyCommand_close)];
}

+ (UIKeyCommand*)cr_showSettings {
  return [self keyCommandWithInput:@","
                     modifierFlags:Command
                            action:@selector(keyCommand_showSettings)];
}

+ (UIKeyCommand*)cr_stop {
  return [self keyCommandWithInput:@"."
                     modifierFlags:Command
                            action:@selector(keyCommand_stop)];
}

+ (UIKeyCommand*)cr_showHelp {
  return [self keyCommandWithInput:@"?"
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showHelp)];
}

+ (UIKeyCommand*)cr_showDownloadsFolder {
  return [self keyCommandWithInput:@"j"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_showDownloadsFolder)];
}

+ (UIKeyCommand*)cr_showDownloadsFolder_2 {
  return [self keyCommandWithInput:@"l"
                     modifierFlags:AltCommand
                            action:@selector(keyCommand_showDownloadsFolder)];
}

+ (UIKeyCommand*)cr_showTab0 {
  return [self keyCommandWithInput:@"1"
                     modifierFlags:Command
                            action:@selector(keyCommand_showTab0)];
}

+ (UIKeyCommand*)cr_showTab1 {
  return [self keyCommandWithInput:@"2"
                     modifierFlags:Command
                            action:@selector(keyCommand_showTab1)];
}

+ (UIKeyCommand*)cr_showTab2 {
  return [self keyCommandWithInput:@"3"
                     modifierFlags:Command
                            action:@selector(keyCommand_showTab2)];
}

+ (UIKeyCommand*)cr_showTab3 {
  return [self keyCommandWithInput:@"4"
                     modifierFlags:Command
                            action:@selector(keyCommand_showTab3)];
}

+ (UIKeyCommand*)cr_showTab4 {
  return [self keyCommandWithInput:@"5"
                     modifierFlags:Command
                            action:@selector(keyCommand_showTab4)];
}

+ (UIKeyCommand*)cr_showTab5 {
  return [self keyCommandWithInput:@"6"
                     modifierFlags:Command
                            action:@selector(keyCommand_showTab5)];
}

+ (UIKeyCommand*)cr_showTab6 {
  return [self keyCommandWithInput:@"7"
                     modifierFlags:Command
                            action:@selector(keyCommand_showTab6)];
}

+ (UIKeyCommand*)cr_showTab7 {
  return [self keyCommandWithInput:@"8"
                     modifierFlags:Command
                            action:@selector(keyCommand_showTab7)];
}

+ (UIKeyCommand*)cr_showLastTab {
  return [self keyCommandWithInput:@"9"
                     modifierFlags:Command
                            action:@selector(keyCommand_showLastTab)];
}

+ (UIKeyCommand*)cr_reportAnIssue {
  return [self cr_commandWithInput:@"i"
                     modifierFlags:ShiftCommand
                            action:@selector(keyCommand_reportAnIssue)
                           titleID:IDS_IOS_KEYBOARD_REPORT_AN_ISSUE];
}

+ (UIKeyCommand*)cr_reportAnIssue_2 {
  return [self keyCommandWithInput:@"i"
                     modifierFlags:AltShiftCommand
                            action:@selector(keyCommand_reportAnIssue)];
}

#pragma mark - Symbolic Description

- (NSString*)cr_symbolicDescription {
  NSMutableString* description = [NSMutableString string];

  if (self.modifierFlags & UIKeyModifierNumericPad)
    [description appendString:@"Num lock "];
  if (self.modifierFlags & UIKeyModifierControl)
    [description appendString:@"⌃"];
  if (self.modifierFlags & UIKeyModifierAlternate)
    [description appendString:@"⌥"];
  if (self.modifierFlags & UIKeyModifierShift)
    [description appendString:@"⇧"];
  if (self.modifierFlags & UIKeyModifierAlphaShift)
    [description appendString:@"⇪"];
  if (self.modifierFlags & UIKeyModifierCommand)
    [description appendString:@"⌘"];

  if ([self.input isEqualToString:@"\b"])
    [description appendString:@"⌫"];
  else if ([self.input isEqualToString:@"\r"])
    [description appendString:@"↵"];
  else if ([self.input isEqualToString:@"\t"])
    [description appendString:@"⇥"];
  else if ([self.input isEqualToString:UIKeyInputUpArrow])
    [description appendString:@"↑"];
  else if ([self.input isEqualToString:UIKeyInputDownArrow])
    [description appendString:@"↓"];
  else if ([self.input isEqualToString:UIKeyInputLeftArrow])
    [description appendString:@"←"];
  else if ([self.input isEqualToString:UIKeyInputRightArrow])
    [description appendString:@"→"];
  else if ([self.input isEqualToString:UIKeyInputEscape])
    [description appendString:@"⎋"];
  else if ([self.input isEqualToString:@" "])
    [description appendString:@"␣"];
  else
    [description appendString:[self.input uppercaseString]];
  return description;
}

#pragma mark - Private

+ (instancetype)cr_commandWithInput:(NSString*)input
                      modifierFlags:(UIKeyModifierFlags)modifierFlags
                             action:(SEL)action
                            titleID:(int)messageID {
  UIKeyCommand* keyCommand =
      [self commandWithTitle:l10n_util::GetNSStringWithFixup(messageID)
                       image:nil
                      action:action
                       input:input
               modifierFlags:modifierFlags
                propertyList:nil];
  keyCommand.discoverabilityTitle = keyCommand.title;
  return keyCommand;
}

@end
