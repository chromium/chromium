// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation KeyCommandsProvider

- (NSArray*)
    keyCommandsForConsumer:(id<KeyCommandsPlumbing>)consumer
        baseViewController:(UIViewController*)baseViewController
                dispatcher:
                    (id<ApplicationCommands, BrowserCommands, OmniboxFocuser>)
                        dispatcher
               editingText:(BOOL)editingText {
  __weak id<KeyCommandsPlumbing> weakConsumer = consumer;
  __weak UIViewController* weakBaseViewController = baseViewController;
  __weak id<ApplicationCommands, BrowserCommands, OmniboxFocuser>
      weakDispatcher = dispatcher;

  // Block to have the tab model open the tab at |index|, if there is one.
  void (^focusTab)(NSUInteger) = ^(NSUInteger index) {
    [weakConsumer focusTabAtIndex:index];
  };

  const BOOL hasTabs = [consumer tabsCount] > 0;

  const BOOL useRTLLayout = UseRTLLayout();

  // Blocks for navigating forward/back.
  void (^browseLeft)();
  void (^browseRight)();
  if (useRTLLayout) {
    browseLeft = ^{
      if ([weakConsumer canGoForward])
        [weakDispatcher goForward];
    };
    browseRight = ^{
      if ([weakConsumer canGoBack])
        [weakDispatcher goBack];
    };
  } else {
    browseLeft = ^{
      if ([weakConsumer canGoBack])
        [weakDispatcher goBack];
    };
    browseRight = ^{
      if ([weakConsumer canGoForward])
        [weakDispatcher goForward];
    };
  }

  // New tab blocks.
  void (^newTab)() = ^{
    OpenNewTabCommand* newTabCommand = [OpenNewTabCommand command];
    newTabCommand.shouldFocusOmnibox = YES;
    [weakDispatcher openURLInNewTab:newTabCommand];
  };

  void (^newIncognitoTab)() = ^{
    OpenNewTabCommand* newIncognitoTabCommand =
        [OpenNewTabCommand incognitoTabCommand];
    newIncognitoTabCommand.shouldFocusOmnibox = YES;
    [weakDispatcher openURLInNewTab:newIncognitoTabCommand];
  };

  const int browseLeftDescriptionID = useRTLLayout
                                          ? IDS_IOS_KEYBOARD_HISTORY_FORWARD
                                          : IDS_IOS_KEYBOARD_HISTORY_BACK;
  const int browseRightDescriptionID = useRTLLayout
                                           ? IDS_IOS_KEYBOARD_HISTORY_BACK
                                           : IDS_IOS_KEYBOARD_HISTORY_FORWARD;

  // Initialize the array of commands with an estimated capacity.
  NSMutableArray* keyCommands = [NSMutableArray arrayWithCapacity:32];

  // List the commands that always appear in the HUD. They appear in the HUD
  // since they have titles.
  [keyCommands addObjectsFromArray:@[
    [UIKeyCommand cr_keyCommandWithInput:@"t"
                           modifierFlags:UIKeyModifierCommand
                                   title:l10n_util::GetNSStringWithFixup(
                                             IDS_IOS_TOOLS_MENU_NEW_TAB)
                                  action:newTab],
    [UIKeyCommand
        cr_keyCommandWithInput:@"n"
                 modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                         title:l10n_util::GetNSStringWithFixup(
                                   IDS_IOS_TOOLS_MENU_NEW_INCOGNITO_TAB)
                        action:newIncognitoTab],
    [UIKeyCommand
        cr_keyCommandWithInput:@"t"
                 modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                         title:l10n_util::GetNSStringWithFixup(
                                   IDS_IOS_KEYBOARD_REOPEN_CLOSED_TAB)
                        action:^{
                          [weakConsumer reopenClosedTab];
                        }],
  ]];

  // List the commands that only appear when there is at least a tab. When they
  // appear, they are in the HUD since they have titles.
  if (hasTabs) {
    if ([consumer isFindInPageAvailable]) {
      [keyCommands addObjectsFromArray:@[

        [UIKeyCommand
            cr_keyCommandWithInput:@"f"
                     modifierFlags:UIKeyModifierCommand
                             title:l10n_util::GetNSStringWithFixup(
                                       IDS_IOS_TOOLS_MENU_FIND_IN_PAGE)
                            action:^{
                              [weakDispatcher showFindInPage];
                            }],
        [UIKeyCommand cr_keyCommandWithInput:@"g"
                               modifierFlags:UIKeyModifierCommand
                                       title:nil
                                      action:^{
                                        [weakDispatcher findNextStringInPage];
                                      }],
        [UIKeyCommand
            cr_keyCommandWithInput:@"g"
                     modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                             title:nil
                            action:^{
                              [weakDispatcher findPreviousStringInPage];
                            }]
      ]];
    }

    [keyCommands addObjectsFromArray:@[
      [UIKeyCommand cr_keyCommandWithInput:@"l"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_IOS_KEYBOARD_OPEN_LOCATION)
                                    action:^{
                                      [weakDispatcher focusOmnibox];
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"w"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_IOS_TOOLS_MENU_CLOSE_TAB)
                                    action:^{
                                      // -closeCurrentTab might destroy the
                                      // object that implements this shortcut
                                      // (BVC), so this selector might not be
                                      // registered with the dispatcher anymore.
                                      // Check if it's still available. See
                                      // crbug.com/967637 for context.
                                      if ([weakDispatcher
                                              respondsToSelector:@selector
                                              (closeCurrentTab)]) {
                                        [weakDispatcher closeCurrentTab];
                                      }
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"d"
                   modifierFlags:UIKeyModifierCommand
                           title:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_KEYBOARD_BOOKMARK_THIS_PAGE)
                          action:^{
                            [weakDispatcher bookmarkPage];
                          }],
      [UIKeyCommand cr_keyCommandWithInput:@"r"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_IOS_ACCNAME_RELOAD)
                                    action:^{
                                      [weakDispatcher reload];
                                    }],
    ]];

    // Since cmd+left and cmd+right are valid system shortcuts when editing
    // text, don't register those if text is being edited.
    if (!editingText) {
      [keyCommands addObjectsFromArray:@[
        [UIKeyCommand cr_keyCommandWithInput:UIKeyInputLeftArrow
                               modifierFlags:UIKeyModifierCommand
                                       title:l10n_util::GetNSStringWithFixup(
                                                 browseLeftDescriptionID)
                                      action:browseLeft],
        [UIKeyCommand cr_keyCommandWithInput:UIKeyInputRightArrow
                               modifierFlags:UIKeyModifierCommand
                                       title:l10n_util::GetNSStringWithFixup(
                                                 browseRightDescriptionID)
                                      action:browseRight],
      ]];
    }

    NSString* voiceSearchTitle = l10n_util::GetNSStringWithFixup(
        IDS_IOS_VOICE_SEARCH_KEYBOARD_DISCOVERY_TITLE);
    [keyCommands addObjectsFromArray:@[
      [UIKeyCommand cr_keyCommandWithInput:@"y"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_HISTORY_SHOW_HISTORY)
                                    action:^{
                                      [weakDispatcher showHistory];
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"."
                   modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                           title:voiceSearchTitle
                          action:^{
                            UIView* baseView = baseViewController.view;
                            [[NamedGuide guideWithName:kVoiceSearchButtonGuide
                                                  view:baseView]
                                resetConstraints];
                            [weakDispatcher startVoiceSearch];
                          }],
    ]];
  }

  // List the commands that don't appear in the HUD but are always present.
  [keyCommands addObjectsFromArray:@[
    [UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                           modifierFlags:Cr_UIKeyModifierNone
                                   title:nil
                                  action:^{
                                    [weakDispatcher dismissModalDialogs];
                                  }],
    [UIKeyCommand cr_keyCommandWithInput:@"n"
                           modifierFlags:UIKeyModifierCommand
                                   title:nil
                                  action:newTab],
    [UIKeyCommand cr_keyCommandWithInput:@","
                           modifierFlags:UIKeyModifierCommand
                                   title:nil
                                  action:^{
                                    [weakDispatcher
                                        showSettingsFromViewController:
                                            weakBaseViewController];
                                  }],
  ]];

  // List the commands that don't appear in the HUD and only appear when there
  // is at least a tab.
  if (hasTabs) {
    [keyCommands addObjectsFromArray:@[
      [UIKeyCommand cr_keyCommandWithInput:@"["
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:browseLeft],
      [UIKeyCommand cr_keyCommandWithInput:@"]"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:browseRight],
      [UIKeyCommand cr_keyCommandWithInput:@"."
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      [weakDispatcher stopLoading];
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"?"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      [weakDispatcher showHelpPage];
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"1"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(0);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"2"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(1);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"3"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(2);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"4"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(3);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"5"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(4);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"6"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(5);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"7"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(6);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"8"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab(7);
                                    }],
      [UIKeyCommand cr_keyCommandWithInput:@"9"
                             modifierFlags:UIKeyModifierCommand
                                     title:nil
                                    action:^{
                                      focusTab([weakConsumer tabsCount] - 1);
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:UIKeyInputLeftArrow
                   modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate
                           title:nil
                          action:^{
                            [weakConsumer focusPreviousTab];
                          }],
      [UIKeyCommand
          cr_keyCommandWithInput:UIKeyInputRightArrow
                   modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate
                           title:nil
                          action:^{
                            [weakConsumer focusNextTab];
                          }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"\t"
                   modifierFlags:UIKeyModifierControl | UIKeyModifierShift
                           title:nil
                          action:^{
                            [weakConsumer focusPreviousTab];
                          }],
      [UIKeyCommand cr_keyCommandWithInput:@"\t"
                             modifierFlags:UIKeyModifierControl
                                     title:nil
                                    action:^{
                                      [weakConsumer focusNextTab];
                                    }],
    ]];
  }

  return keyCommands;
}

@end
