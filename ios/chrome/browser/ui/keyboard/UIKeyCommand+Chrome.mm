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

UIKeyModifierFlags KeyModifierNone = 0;
UIKeyModifierFlags KeyModifierCommand = UIKeyModifierCommand;
UIKeyModifierFlags KeyModifierControl = UIKeyModifierControl;
UIKeyModifierFlags KeyModifierAltCommand =
    UIKeyModifierAlternate | UIKeyModifierCommand;
UIKeyModifierFlags KeyModifierShiftCommand =
    UIKeyModifierShift | UIKeyModifierCommand;
UIKeyModifierFlags KeyModifierShiftAltCommand =
    UIKeyModifierShift | UIKeyModifierAlternate | UIKeyModifierCommand;
UIKeyModifierFlags KeyModifierControlShift =
    UIKeyModifierControl | UIKeyModifierShift;

@implementation UIKeyCommand (Chrome)

#pragma mark - Specific Keyboard Commands

+ (UIKeyCommand*)cr_openNewTab {
  return [self cr_commandWithInput:@"t"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_openNewTab)
                           titleID:IDS_IOS_TOOLS_MENU_NEW_TAB];
}

+ (UIKeyCommand*)cr_openNewTab_2 {
  return [self keyCommandWithInput:@"n"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_openNewTab)];
}

+ (UIKeyCommand*)cr_openNewIncognitoTab {
  return [self cr_commandWithInput:@"n"
                     modifierFlags:KeyModifierShiftCommand
                            action:@selector(keyCommand_openNewIncognitoTab)
                           titleID:IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB];
}

+ (UIKeyCommand*)cr_reopenClosedTab {
  return [self cr_commandWithInput:@"t"
                     modifierFlags:KeyModifierShiftCommand
                            action:@selector(keyCommand_reopenClosedTab)
                           titleID:IDS_IOS_KEYBOARD_REOPEN_CLOSED_TAB];
}

+ (UIKeyCommand*)cr_openFindInPage {
  return [self cr_commandWithInput:@"f"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_openFindInPage)
                           titleID:IDS_IOS_TOOLS_MENU_FIND_IN_PAGE];
}

+ (UIKeyCommand*)cr_findNextStringInPage {
  return [self keyCommandWithInput:@"g"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_findNextStringInPage)];
}

+ (UIKeyCommand*)cr_findPreviousStringInPage {
  return
      [self keyCommandWithInput:@"g"
                  modifierFlags:KeyModifierShiftCommand
                         action:@selector(keyCommand_findPreviousStringInPage)];
}

+ (UIKeyCommand*)cr_focusOmnibox {
  return [self cr_commandWithInput:@"l"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_focusOmnibox)
                           titleID:IDS_IOS_KEYBOARD_OPEN_LOCATION];
}

+ (UIKeyCommand*)cr_closeTab {
  return [self cr_commandWithInput:@"w"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_closeTab)
                           titleID:IDS_IOS_TOOLS_MENU_CLOSE_TAB];
}

+ (UIKeyCommand*)cr_showNextTab {
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
  return [self cr_commandWithInput:arrowNext
                     modifierFlags:KeyModifierAltCommand
                            action:@selector(keyCommand_showNextTab)
                           titleID:IDS_IOS_KEYBOARD_NEXT_TAB];
}

+ (UIKeyCommand*)cr_showPreviousTab {
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
  return [self cr_commandWithInput:arrowPrevious
                     modifierFlags:KeyModifierAltCommand
                            action:@selector(keyCommand_showPreviousTab)
                           titleID:IDS_IOS_KEYBOARD_PREVIOUS_TAB];
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
                     modifierFlags:KeyModifierCommand
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
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showPreviousTab)];
}

+ (UIKeyCommand*)cr_showNextTab_3 {
  // TODO(crbug.com/1278594): Doesn't work on iOS 15+.
  return [self keyCommandWithInput:@"\t"
                     modifierFlags:KeyModifierControl
                            action:@selector(keyCommand_showNextTab)];
}

+ (UIKeyCommand*)cr_showPreviousTab_3 {
  // TODO(crbug.com/1278594): Doesn't work on iOS 15+.
  return [self keyCommandWithInput:@"\t"
                     modifierFlags:KeyModifierControlShift
                            action:@selector(keyCommand_showPreviousTab)];
}

+ (UIKeyCommand*)cr_addToBookmarks {
  return [self cr_commandWithInput:@"d"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_addToBookmarks)
                           titleID:IDS_IOS_KEYBOARD_ADD_TO_BOOKMARKS];
}

+ (UIKeyCommand*)cr_reload {
  return [self cr_commandWithInput:@"r"
                     modifierFlags:KeyModifierCommand
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
                     modifierFlags:KeyModifierCommand
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
                     modifierFlags:KeyModifierCommand
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
                     modifierFlags:KeyModifierCommand
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
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_goForward)];
}

+ (UIKeyCommand*)cr_showHistory {
  return [self cr_commandWithInput:@"y"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showHistory)
                           titleID:IDS_HISTORY_SHOW_HISTORY];
}

+ (UIKeyCommand*)cr_startVoiceSearch {
  return
      [self cr_commandWithInput:@"."
                  modifierFlags:KeyModifierShiftCommand
                         action:@selector(keyCommand_startVoiceSearch)
                        titleID:IDS_IOS_VOICE_SEARCH_KEYBOARD_DISCOVERY_TITLE];
}

+ (UIKeyCommand*)cr_dismissModalDialogs {
  return [self keyCommandWithInput:UIKeyInputEscape
                     modifierFlags:KeyModifierNone
                            action:@selector(keyCommand_dismissModalDialogs)];
}

+ (UIKeyCommand*)cr_showSettings {
  return [self keyCommandWithInput:@","
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showSettings)];
}

+ (UIKeyCommand*)cr_stop {
  return [self keyCommandWithInput:@"."
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_stop)];
}

+ (UIKeyCommand*)cr_showHelpPage {
  return [self keyCommandWithInput:@"?"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showHelpPage)];
}

+ (UIKeyCommand*)cr_showDownloadsFolder {
  return [self keyCommandWithInput:@"j"
                     modifierFlags:KeyModifierShiftCommand
                            action:@selector(keyCommand_showDownloadsFolder)];
}

+ (UIKeyCommand*)cr_showDownloadsFolder_2 {
  return [self keyCommandWithInput:@"l"
                     modifierFlags:KeyModifierAltCommand
                            action:@selector(keyCommand_showDownloadsFolder)];
}

+ (UIKeyCommand*)cr_showTab0 {
  return [self keyCommandWithInput:@"1"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showTab0)];
}

+ (UIKeyCommand*)cr_showTab1 {
  return [self keyCommandWithInput:@"2"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showTab1)];
}

+ (UIKeyCommand*)cr_showTab2 {
  return [self keyCommandWithInput:@"3"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showTab2)];
}

+ (UIKeyCommand*)cr_showTab3 {
  return [self keyCommandWithInput:@"4"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showTab3)];
}

+ (UIKeyCommand*)cr_showTab4 {
  return [self keyCommandWithInput:@"5"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showTab4)];
}

+ (UIKeyCommand*)cr_showTab5 {
  return [self keyCommandWithInput:@"6"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showTab5)];
}

+ (UIKeyCommand*)cr_showTab6 {
  return [self keyCommandWithInput:@"7"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showTab6)];
}

+ (UIKeyCommand*)cr_showTab7 {
  return [self keyCommandWithInput:@"8"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showTab7)];
}

+ (UIKeyCommand*)cr_showLastTab {
  return [self keyCommandWithInput:@"9"
                     modifierFlags:KeyModifierCommand
                            action:@selector(keyCommand_showLastTab)];
}

#pragma mark - Symbolic Description

- (NSString*)cr_symbolicDescription {
  NSMutableString* description = [NSMutableString string];

  if (self.modifierFlags & UIKeyModifierNumericPad)
    [description appendString:@"Num lock "];
  if (self.modifierFlags & UIKeyModifierControl)
    [description appendString:@"⌃"];
  if (self.modifierFlags & UIKeyModifierShift)
    [description appendString:@"⇧"];
  if (self.modifierFlags & UIKeyModifierAlphaShift)
    [description appendString:@"⇪"];
  if (self.modifierFlags & UIKeyModifierAlternate)
    [description appendString:@"⌥"];
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

#pragma mark - Factory

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
