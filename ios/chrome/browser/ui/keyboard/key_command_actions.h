// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_KEYBOARD_KEY_COMMAND_ACTIONS_H_
#define IOS_CHROME_BROWSER_UI_KEYBOARD_KEY_COMMAND_ACTIONS_H_

#import <Foundation/Foundation.h>

// Declares the possible actions from key commands.
// Implementors of these actions are advised to record a UMA when their action
// is called, named after the action: "MobileKeyCommandXxx" for keyCommand_xxx.
@protocol KeyCommandActions <NSObject>

@optional
- (void)keyCommand_openNewTab;
- (void)keyCommand_openNewRegularTab;
- (void)keyCommand_openNewIncognitoTab;
- (void)keyCommand_openNewWindow;
- (void)keyCommand_openNewIncognitoWindow;
- (void)keyCommand_reopenLastClosedTab;
- (void)keyCommand_find;
- (void)keyCommand_findNext;
- (void)keyCommand_findPrevious;
- (void)keyCommand_openLocation;
- (void)keyCommand_closeTab;
- (void)keyCommand_showNextTab;
- (void)keyCommand_showPreviousTab;
- (void)keyCommand_showBookmarks;
- (void)keyCommand_addToBookmarks;
- (void)keyCommand_reload;
- (void)keyCommand_back;
- (void)keyCommand_forward;
- (void)keyCommand_showHistory;
- (void)keyCommand_voiceSearch;
- (void)keyCommand_close;
- (void)keyCommand_showSettings;
- (void)keyCommand_stop;
- (void)keyCommand_showHelp;
- (void)keyCommand_showDownloads;
- (void)keyCommand_showFirstTab;
- (void)keyCommand_showTab2;
- (void)keyCommand_showTab3;
- (void)keyCommand_showTab4;
- (void)keyCommand_showTab5;
- (void)keyCommand_showTab6;
- (void)keyCommand_showTab7;
- (void)keyCommand_showTab8;
- (void)keyCommand_showLastTab;
- (void)keyCommand_reportAnIssue;
- (void)keyCommand_addToReadingList;
- (void)keyCommand_showReadingList;
- (void)keyCommand_goToTabGrid;
- (void)keyCommand_clearBrowsingData;

@end

#endif  // IOS_CHROME_BROWSER_UI_KEYBOARD_KEY_COMMAND_ACTIONS_H_
