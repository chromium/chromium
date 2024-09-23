// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/keyboard/ui_bundled/menu_builder.h"

#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"

// Note: this class can be called very early on in the start process, before
// resource bundles are loaded. This means that to get localized strings, one
// shouldn't use `l10n_util::GetNSString()` and instead should use
// `NSLocalizedString(@"IDS_IOS_MY_STRING", @"")`, with
// `IDS_IOS_MY_STRING` present in the allowlist at
// //ios/chrome/app/resources/chrome_localize_strings_config.plist.

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
    UIKeyCommand.cr_openLocation,
    UIKeyCommand.cr_closeTab,
    UIKeyCommand.cr_voiceSearch,
    UIKeyCommand.cr_closeAll,
  ]];
  [builder insertChildMenu:fileMenu atStartOfMenuForIdentifier:UIMenuFile];

  // Edit
  UIMenu* editMenu = [UIMenu menuWithChildren:@[
    UIKeyCommand.cr_find,
    UIKeyCommand.cr_findNext,
    UIKeyCommand.cr_findPrevious,
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
  UIMenu* historyMenu =
      [UIMenu menuWithTitle:NSLocalizedString(@"IDS_IOS_KEYBOARD_HISTORY", @"")
                   children:@[
                     UIKeyCommand.cr_back,
                     UIKeyCommand.cr_forward,
                     UIKeyCommand.cr_reopenLastClosedTab,
                     UIKeyCommand.cr_showHistory,
                     UIKeyCommand.cr_clearBrowsingData,
                   ]];
  [builder insertSiblingMenu:historyMenu afterMenuForIdentifier:UIMenuView];

  // Bookmarks
  UIMenu* bookmarksMenu = [UIMenu
      menuWithTitle:NSLocalizedString(@"IDS_IOS_KEYBOARD_BOOKMARKS", @"")
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
    UIKeyCommand.cr_select1,
    UIKeyCommand.cr_select9,
    UIKeyCommand.cr_showDownloads,
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
