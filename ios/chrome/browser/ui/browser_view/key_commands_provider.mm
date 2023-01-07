// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view/key_commands_provider.h"

#import "components/sessions/core/tab_restore_service_helper.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/find_in_page/find_tab_helper.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/ui/commands/bookmarks_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/util/keyboard_observer_helper.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/util_swift.h"
#import "ios/chrome/browser/url_loading/url_loading_util.h"
#import "ios/chrome/browser/web/web_navigation_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface KeyCommandsProvider ()

@property(nonatomic, assign) Browser* browser;
@property(nonatomic, assign, readonly)
    WebNavigationBrowserAgent* navigationAgent;

@end

@implementation KeyCommandsProvider

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super init];
  if (self) {
    _browser = browser;
  }
  return self;
}

- (NSArray<UIKeyCommand*>*)keyCommandsWithEditingText:(BOOL)editingText {
  __weak __typeof(self) weakSelf = self;

  // Block to have the tab model open the tab at `index`, if there is one.
  void (^focusTab)(NSUInteger) = ^(NSUInteger index) {
    [weakSelf focusTabAtIndex:index];
  };

  const BOOL hasTabs = [self tabsCount] > 0;

  const BOOL useRTLLayout = UseRTLLayout();

  // Blocks for navigating forward/back.
  void (^browseLeft)();
  void (^browseRight)();
  if (useRTLLayout) {
    browseLeft = ^{
      __typeof(self) strongSelf = weakSelf;
      if (strongSelf.navigationAgent->CanGoForward())
        strongSelf.navigationAgent->GoForward();
    };
    browseRight = ^{
      __typeof(self) strongSelf = weakSelf;
      if (strongSelf.navigationAgent->CanGoBack())
        strongSelf.navigationAgent->GoBack();
    };
  } else {
    browseLeft = ^{
      __typeof(self) strongSelf = weakSelf;
      if (strongSelf.navigationAgent->CanGoBack())
        strongSelf.navigationAgent->GoBack();
    };
    browseRight = ^{
      __typeof(self) strongSelf = weakSelf;
      if (strongSelf.navigationAgent->CanGoForward())
        strongSelf.navigationAgent->GoForward();
    };
  }

  // Blocks for next/previous tab.
  void (^focusTabLeft)();
  void (^focusTabRight)();
  if (useRTLLayout) {
    focusTabLeft = ^{
      [weakSelf focusNextTab];
    };
    focusTabRight = ^{
      [weakSelf focusPreviousTab];
    };
  } else {
    focusTabLeft = ^{
      [weakSelf focusPreviousTab];
    };
    focusTabRight = ^{
      [weakSelf focusNextTab];
    };
  }

  // New tab blocks.
  void (^newTab)() = ^{
    OpenNewTabCommand* newTabCommand = [OpenNewTabCommand command];
    newTabCommand.shouldFocusOmnibox = YES;
    [weakSelf.dispatcher openURLInNewTab:newTabCommand];
  };

  void (^newIncognitoTab)() = ^{
    OpenNewTabCommand* newIncognitoTabCommand =
        [OpenNewTabCommand incognitoTabCommand];
    newIncognitoTabCommand.shouldFocusOmnibox = YES;
    [weakSelf.dispatcher openURLInNewTab:newIncognitoTabCommand];
  };

  const int browseLeftDescriptionID = useRTLLayout
                                          ? IDS_IOS_KEYBOARD_HISTORY_FORWARD
                                          : IDS_IOS_KEYBOARD_HISTORY_BACK;
  const int browseRightDescriptionID = useRTLLayout
                                           ? IDS_IOS_KEYBOARD_HISTORY_BACK
                                           : IDS_IOS_KEYBOARD_HISTORY_FORWARD;

  // Initialize the array of commands with an estimated capacity.
  NSMutableArray<UIKeyCommand*>* keyCommands =
      [NSMutableArray arrayWithCapacity:32];

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
                          [weakSelf reopenClosedTab];
                        }],
  ]];

  // List the commands that only appear when there is at least a tab. When they
  // appear, they are in the HUD since they have titles.
  if (hasTabs) {
    if ([self isFindInPageAvailable]) {
      [keyCommands addObjectsFromArray:@[

        [UIKeyCommand
            cr_keyCommandWithInput:@"f"
                     modifierFlags:UIKeyModifierCommand
                             title:l10n_util::GetNSStringWithFixup(
                                       IDS_IOS_TOOLS_MENU_FIND_IN_PAGE)
                            action:^{
                              [weakSelf.dispatcher openFindInPage];
                            }],
        [UIKeyCommand
            cr_keyCommandWithInput:@"g"
                     modifierFlags:UIKeyModifierCommand
                             title:nil
                            action:^{
                              [weakSelf.dispatcher findNextStringInPage];
                            }],
        [UIKeyCommand
            cr_keyCommandWithInput:@"g"
                     modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                             title:nil
                            action:^{
                              [weakSelf.dispatcher findPreviousStringInPage];
                            }]
      ]];
    }

    [keyCommands addObjectsFromArray:@[
      [UIKeyCommand cr_keyCommandWithInput:@"l"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_IOS_KEYBOARD_OPEN_LOCATION)
                                    action:^{
                                      [weakSelf.omniboxHandler focusOmnibox];
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
                                      if ([weakSelf.dispatcher
                                              respondsToSelector:@selector
                                              (closeCurrentTab)]) {
                                        [weakSelf
                                                .browserCoordinatorCommandsHandler
                                                    closeCurrentTab];
                                      }
                                    }],
    ]];

    // Deal with the multiple next/previous tab commands we have, only one pair
    // of which appears in the HUD. Take RTL into account for the direction.
    const int tabLeftDescriptionID = useRTLLayout
                                          ? IDS_IOS_KEYBOARD_NEXT_TAB
                                          : IDS_IOS_KEYBOARD_PREVIOUS_TAB;
    const int tabRightDescriptionID = useRTLLayout
                                           ? IDS_IOS_KEYBOARD_PREVIOUS_TAB
                                           : IDS_IOS_KEYBOARD_NEXT_TAB;
    NSString* tabLeftTitle = l10n_util::GetNSStringWithFixup(
        tabLeftDescriptionID);
    NSString* tabRightTitle = l10n_util::GetNSStringWithFixup(
        tabRightDescriptionID);
    [keyCommands addObjectsFromArray:@[
      [UIKeyCommand
           cr_keyCommandWithInput:UIKeyInputLeftArrow
                    modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate
                            title:tabLeftTitle
                           action:focusTabLeft],
       [UIKeyCommand
           cr_keyCommandWithInput:UIKeyInputRightArrow
                    modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate
                            title:tabRightTitle
                           action:focusTabRight],
       [UIKeyCommand
           cr_keyCommandWithInput:@"{"
                    modifierFlags:UIKeyModifierCommand
                            title:nil
                           action:focusTabLeft],
       [UIKeyCommand
           cr_keyCommandWithInput:@"}"
                    modifierFlags:UIKeyModifierCommand
                            title:nil
                           action:focusTabRight],
    ]];

    [keyCommands addObjectsFromArray:@[
      [UIKeyCommand
          cr_keyCommandWithInput:@"d"
                   modifierFlags:UIKeyModifierCommand
                           title:l10n_util::GetNSStringWithFixup(
                                     IDS_IOS_KEYBOARD_BOOKMARK_THIS_PAGE)
                          action:^{
                            if (weakSelf.browser) {
                              web::WebState* currentWebState =
                                  weakSelf.browser->GetWebStateList()
                                      ->GetActiveWebState();
                              if (currentWebState) {
                                BookmarkAddCommand* command =
                                    [[BookmarkAddCommand alloc]
                                            initWithWebState:currentWebState
                                        presentFolderChooser:NO];
                                [weakSelf.bookmarksCommandsHandler
                                    bookmark:command];
                              }
                            }
                          }],
      [UIKeyCommand cr_keyCommandWithInput:@"r"
                             modifierFlags:UIKeyModifierCommand
                                     title:l10n_util::GetNSStringWithFixup(
                                               IDS_IOS_ACCNAME_RELOAD)
                                    action:^{
                                      weakSelf.navigationAgent->Reload();
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
                                      [weakSelf.dispatcher showHistory];
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"."
                   modifierFlags:UIKeyModifierCommand | UIKeyModifierShift
                           title:voiceSearchTitle
                          action:^{
                            [LayoutGuideCenterForBrowser(weakSelf.browser)
                                referenceView:nil
                                    underName:kVoiceSearchButtonGuide];
                            [weakSelf.dispatcher startVoiceSearch];
                          }],
    ]];
  }

  if (self.canDismissModals) {
    [keyCommands
        addObject:[UIKeyCommand cr_keyCommandWithInput:UIKeyInputEscape
                                         modifierFlags:Cr_UIKeyModifierNone
                                                 title:nil
                                                action:^{
                                                  [weakSelf.dispatcher
                                                          dismissModalDialogs];
                                                }]];
  }

  // List the commands that don't appear in the HUD but are always present.
  [keyCommands addObjectsFromArray:@[
    [UIKeyCommand cr_keyCommandWithInput:@"n"
                           modifierFlags:UIKeyModifierCommand
                                   title:nil
                                  action:newTab],
    [UIKeyCommand cr_keyCommandWithInput:@","
                           modifierFlags:UIKeyModifierCommand
                                   title:nil
                                  action:^{
                                    [weakSelf.dispatcher
                                        showSettingsFromViewController:
                                            weakSelf.baseViewController];
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
                                      weakSelf.navigationAgent->StopLoading();
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"?"
                   modifierFlags:UIKeyModifierCommand
                           title:nil
                          action:^{
                            [weakSelf.browserCoordinatorCommandsHandler
                                    showHelpPage];
                          }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"l"
                   modifierFlags:UIKeyModifierCommand | UIKeyModifierAlternate
                           title:nil
                          action:^{
                            [weakSelf.browserCoordinatorCommandsHandler
                                    showDownloadsFolder];
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
                                      focusTab([weakSelf tabsCount] - 1);
                                    }],
      [UIKeyCommand
          cr_keyCommandWithInput:@"\t"
                   modifierFlags:UIKeyModifierControl | UIKeyModifierShift
                           title:nil
                          action:^{
                            [weakSelf focusPreviousTab];
                          }],
      [UIKeyCommand cr_keyCommandWithInput:@"\t"
                             modifierFlags:UIKeyModifierControl
                                     title:nil
                                    action:^{
                                      [weakSelf focusNextTab];
                                    }],
    ]];
  }

  return [keyCommands copy];
}

#pragma mark - Private

- (WebNavigationBrowserAgent*)navigationAgent {
  return WebNavigationBrowserAgent::FromBrowser(self.browser);
}

- (BOOL)isFindInPageAvailable {
  web::WebState* currentWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!currentWebState) {
    return NO;
  }

  FindTabHelper* helper = FindTabHelper::FromWebState(currentWebState);
  return (helper && helper->CurrentPageSupportsFindInPage());
}

- (NSUInteger)tabsCount {
  return self.browser->GetWebStateList()->count();
}

- (void)focusTabAtIndex:(NSUInteger)index {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (webStateList->ContainsIndex(index)) {
    webStateList->ActivateWebStateAt(static_cast<int>(index));
  }
}

- (void)focusNextTab {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (!webStateList)
    return;

  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex)
    return;

  // If the active index isn't the last index, activate the next index.
  // (the last index is always `count() - 1`).
  // Otherwise activate the first index.
  if (activeIndex < (webStateList->count() - 1)) {
    webStateList->ActivateWebStateAt(activeIndex + 1);
  } else {
    webStateList->ActivateWebStateAt(0);
  }
}

- (void)focusPreviousTab {
  WebStateList* webStateList = self.browser->GetWebStateList();
  if (!webStateList)
    return;

  int activeIndex = webStateList->active_index();
  if (activeIndex == WebStateList::kInvalidIndex)
    return;

  // If the active index isn't the first index, activate the prior index.
  // Otherwise index the last index (`count() - 1`).
  if (activeIndex > 0) {
    webStateList->ActivateWebStateAt(activeIndex - 1);
  } else {
    webStateList->ActivateWebStateAt(webStateList->count() - 1);
  }
}

- (void)reopenClosedTab {
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  sessions::TabRestoreService* const tabRestoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(browserState);
  if (!tabRestoreService || tabRestoreService->entries().empty())
    return;

  const std::unique_ptr<sessions::TabRestoreService::Entry>& entry =
      tabRestoreService->entries().front();
  // Only handle the TAB type.
  // TODO(crbug.com/1056596) : Support WINDOW restoration under multi-window.
  if (entry->type != sessions::TabRestoreService::TAB)
    return;

  [self.dispatcher openURLInNewTab:[OpenNewTabCommand command]];
  RestoreTab(entry->id, WindowOpenDisposition::CURRENT_TAB, self.browser);
}

@end
