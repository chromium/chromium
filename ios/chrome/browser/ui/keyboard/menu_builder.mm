// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/keyboard/menu_builder.h"

#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation MenuBuilder

+ (void)buildMainMenuWithBuilder:(id<UIMenuBuilder>)builder {
  // Only configure the builder for the main command system, not contextual
  // menus.
  if (builder.system != UIMenuSystem.mainSystem)
    return;

  // File
  UIMenu* fileMenu = [UIMenu menuWithChildren:@[
    UIKeyCommand.cr_openNewTab,
    UIKeyCommand.cr_openNewIncognitoTab,
    UIKeyCommand.cr_openNewWindow,
    UIKeyCommand.cr_openNewIncognitoWindow,
    UIKeyCommand.cr_focusOmnibox,
    UIKeyCommand.cr_closeTab,
    UIKeyCommand.cr_startVoiceSearch,
  ]];
  [builder insertChildMenu:fileMenu atStartOfMenuForIdentifier:UIMenuFile];

  // Edit
  UIMenu* editMenu = [UIMenu menuWithChildren:@[
    UIKeyCommand.cr_openFindInPage,
    UIKeyCommand.cr_findNextStringInPage,
    UIKeyCommand.cr_findPreviousStringInPage,
  ]];
  // Remove the conflicting Find commands.
  [builder removeMenuForIdentifier:UIMenuFind];
  [builder insertChildMenu:editMenu atStartOfMenuForIdentifier:UIMenuEdit];

  // View
  UIMenu* viewMenu = [UIMenu menuWithChildren:@[
    UIKeyCommand.cr_stop,
    UIKeyCommand.cr_reload,
    UIKeyCommand.cr_goToTabGrid,
  ]];
  [builder insertChildMenu:viewMenu atStartOfMenuForIdentifier:UIMenuView];

  // History
  UIMenu* historyMenu = [UIMenu menuWithTitle:@"History"
                                     children:@[
                                       UIKeyCommand.cr_goBack,
                                       UIKeyCommand.cr_goForward,
                                       UIKeyCommand.cr_reopenLastClosedTab,
                                       UIKeyCommand.cr_showHistory,
                                       UIKeyCommand.cr_clearBrowsingData,
                                     ]];
  [builder insertSiblingMenu:historyMenu afterMenuForIdentifier:UIMenuView];

  // Bookmarks
  UIMenu* bookmarksMenu = [UIMenu menuWithTitle:@"Bookmarks"
                                       children:@[
                                         UIKeyCommand.cr_showBookmarks,
                                         UIKeyCommand.cr_addToBookmarks,
                                         UIKeyCommand.cr_showReadingList,
                                         UIKeyCommand.cr_addToReadingList,
                                       ]];
  [builder insertSiblingMenu:bookmarksMenu
      afterMenuForIdentifier:historyMenu.identifier];

  // Window
  UIMenu* windowMenu = [UIMenu menuWithChildren:@[
    UIKeyCommand.cr_showNextTab,
    UIKeyCommand.cr_showPreviousTab,
    UIKeyCommand.cr_showTab0,
    UIKeyCommand.cr_showLastTab,
    UIKeyCommand.cr_showDownloadsFolder,
    UIKeyCommand.cr_showSettings,
  ]];
  [builder insertChildMenu:windowMenu atStartOfMenuForIdentifier:UIMenuWindow];

  // Help
  UIMenu* helpMenu = [UIMenu menuWithChildren:@[
    UIKeyCommand.cr_showHelp,
    UIKeyCommand.cr_reportAnIssue,
  ]];
  [builder insertChildMenu:helpMenu atStartOfMenuForIdentifier:UIMenuHelp];
}

@end
