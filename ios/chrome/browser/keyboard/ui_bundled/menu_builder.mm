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
  if (builder.system != UIMenuSystem.mainSystem) {
    return;
  }

  if (@available(iOS 26, *)) {
    // Application: Replace the Application elements.
    NSArray<UIMenuElement*>* applicationElements = @[
      UIKeyCommand.cr_showSettings,
      UIKeyCommand.cr_clearBrowsingData,
    ];
    // Replace the existing elements, as none are relevant for Chrome.
    [builder replaceChildrenOfMenuForIdentifier:UIMenuApplication
                              fromChildrenBlock:^(NSArray<UIMenuElement*>* _) {
                                return applicationElements;
                              }];

    // File: Replace the File elements.
    NSArray<UIMenuElement*>* fileElements = @[
      UIKeyCommand.cr_openNewTab,
      UIKeyCommand.cr_openNewIncognitoTab,
      UIKeyCommand.cr_openNewWindow,
      UIKeyCommand.cr_openNewIncognitoWindow,
      UIKeyCommand.cr_openLocation,
      UIKeyCommand.cr_voiceSearch,
    ];
    // Replace the existing elements, as none are relevant for Chrome.
    [builder replaceChildrenOfMenuForIdentifier:UIMenuFile
                              fromChildrenBlock:^(NSArray<UIMenuElement*>* _) {
                                return fileElements;
                              }];

    // File: Add the Close elements as an inlined submenu.
    UIMenu* closeMenu = [UIMenu menuWithTitle:@""
                                        image:nil
                                   identifier:nil
                                      options:UIMenuOptionsDisplayInline
                                     children:@[
                                       UIKeyCommand.cr_closeTab,
                                       UIKeyCommand.cr_closeAll,
                                     ]];
    [builder insertChildMenu:closeMenu atEndOfMenuForIdentifier:UIMenuFile];

    // Edit: Replace the Find elements.
    NSArray<UIMenuElement*>* findElements = @[
      UIKeyCommand.cr_find,
      UIKeyCommand.cr_findNext,
      UIKeyCommand.cr_findPrevious,
    ];
    [builder replaceChildrenOfMenuForIdentifier:UIMenuFind
                              fromChildrenBlock:^(NSArray<UIMenuElement*>* _) {
                                return findElements;
                              }];

    // Format: Remove the system entry to format text.
    [builder removeMenuForIdentifier:UIMenuFormat];

    // View: Add elements.
    NSArray<UIMenuElement*>* viewElements = @[
      UIKeyCommand.cr_stop,
      UIKeyCommand.cr_reload,
      UIKeyCommand.cr_goToTabGrid,
    ];
    [self insertElements:viewElements
        atStartOfMenuForIdentifier:UIMenuView
                         inBuilder:builder];

    // History: Add new menu.
    UIMenu* historyMenu = [UIMenu
        menuWithTitle:NSLocalizedString(@"IDS_IOS_KEYBOARD_HISTORY", @"")
             children:@[
               UIKeyCommand.cr_back,
               UIKeyCommand.cr_forward,
               UIKeyCommand.cr_reopenLastClosedTab,
               UIKeyCommand.cr_showHistory,
             ]];
    [builder insertSiblingMenu:historyMenu afterMenuForIdentifier:UIMenuView];

    // Bookmarks: Add new menu.
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

    // Window: Add elements.
    NSArray<UIMenuElement*>* windowElements = @[
      UIKeyCommand.cr_showNextTab,
      UIKeyCommand.cr_showPreviousTab,
      UIKeyCommand.cr_select1,
      UIKeyCommand.cr_select2,
      UIKeyCommand.cr_select3,
      UIKeyCommand.cr_select9,
      UIKeyCommand.cr_showDownloads,
    ];
    [self insertElements:windowElements
        atStartOfMenuForIdentifier:UIMenuWindow
                         inBuilder:builder];

    // Help: Add elements.
    NSArray<UIMenuElement*>* helpElements = @[
      UIKeyCommand.cr_showHelp,
      UIKeyCommand.cr_reportAnIssue,
    ];
    [self insertElements:helpElements
        atStartOfMenuForIdentifier:UIMenuHelp
                         inBuilder:builder];
  } else {
    // Application: Remove the Application menu, as it contains a system entry
    // to open the Settings app, as it conflicts with the in-app settings key
    // command.
    [builder removeMenuForIdentifier:UIMenuApplication];

    // File: Add elements.
    NSArray<UIMenuElement*>* fileElements = @[
      UIKeyCommand.cr_openNewTab,
      UIKeyCommand.cr_openNewIncognitoTab,
      UIKeyCommand.cr_openNewWindow,
      UIKeyCommand.cr_openNewIncognitoWindow,
      UIKeyCommand.cr_openLocation,
      UIKeyCommand.cr_closeTab,
      UIKeyCommand.cr_voiceSearch,
      UIKeyCommand.cr_closeAll,
    ];
    [self insertElements:fileElements
        atStartOfMenuForIdentifier:UIMenuFile
                         inBuilder:builder];

    // Edit: Replace the Find actions.
    NSArray<UIMenuElement*>* findElements = @[
      UIKeyCommand.cr_find,
      UIKeyCommand.cr_findNext,
      UIKeyCommand.cr_findPrevious,
    ];
    [builder replaceChildrenOfMenuForIdentifier:UIMenuFind
                              fromChildrenBlock:^(NSArray<UIMenuElement*>* _) {
                                return findElements;
                              }];

    // Format: Remove the system entry to format text.
    [builder removeMenuForIdentifier:UIMenuFormat];

    // View: Add elements.
    NSArray<UIMenuElement*>* viewElements = @[
      UIKeyCommand.cr_stop,
      UIKeyCommand.cr_reload,
      UIKeyCommand.cr_goToTabGrid,
    ];
    [self insertElements:viewElements
        atStartOfMenuForIdentifier:UIMenuView
                         inBuilder:builder];

    // History: Add new menu.
    UIMenu* historyMenu = [UIMenu
        menuWithTitle:NSLocalizedString(@"IDS_IOS_KEYBOARD_HISTORY", @"")
             children:@[
               UIKeyCommand.cr_back,
               UIKeyCommand.cr_forward,
               UIKeyCommand.cr_reopenLastClosedTab,
               UIKeyCommand.cr_showHistory,
               UIKeyCommand.cr_clearBrowsingData,
             ]];
    [builder insertSiblingMenu:historyMenu afterMenuForIdentifier:UIMenuView];

    // Bookmarks: Add new menu.
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

    // Window: Add elements.
    NSArray<UIMenuElement*>* windowElements = @[
      UIKeyCommand.cr_showNextTab,
      UIKeyCommand.cr_showPreviousTab,
      UIKeyCommand.cr_select1,
      UIKeyCommand.cr_select2,
      UIKeyCommand.cr_select3,
      UIKeyCommand.cr_select9,
      UIKeyCommand.cr_showDownloads,
      UIKeyCommand.cr_showSettings,
    ];
    [self insertElements:windowElements
        atStartOfMenuForIdentifier:UIMenuWindow
                         inBuilder:builder];

    // Help: Add elements.
    NSArray<UIMenuElement*>* helpElements = @[
      UIKeyCommand.cr_showHelp,
      UIKeyCommand.cr_reportAnIssue,
    ];
    [self insertElements:helpElements
        atStartOfMenuForIdentifier:UIMenuHelp
                         inBuilder:builder];
  }
}

#pragma mark - Private

+ (void)insertElements:(NSArray<UIMenuElement*>*)childElements
    atStartOfMenuForIdentifier:(UIMenuIdentifier)parentIdentifier
                     inBuilder:(id<UIMenuBuilder>)builder {
  if (@available(iOS 26, *)) {
    [builder insertElements:childElements
        atStartOfMenuForIdentifier:parentIdentifier];
    return;
  }
  [builder
      replaceChildrenOfMenuForIdentifier:parentIdentifier
                       fromChildrenBlock:^(NSArray<UIMenuElement*>* elements) {
                         return [childElements
                             arrayByAddingObjectsFromArray:elements];
                       }];
}

@end
