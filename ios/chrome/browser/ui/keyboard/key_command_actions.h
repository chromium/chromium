// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_KEYBOARD_KEY_COMMAND_ACTIONS_H_
#define IOS_CHROME_BROWSER_UI_KEYBOARD_KEY_COMMAND_ACTIONS_H_

#import <Foundation/Foundation.h>

// Declares the possible actions from key commands.
@protocol KeyCommandActions <NSObject>

@optional
- (void)keyCommand_openNewTab;
- (void)keyCommand_openNewRegularTab;
- (void)keyCommand_openNewIncognitoTab;
- (void)keyCommand_openNewWindow;
- (void)keyCommand_reopenLastClosedTab;
- (void)keyCommand_openFindInPage;
- (void)keyCommand_findNextStringInPage;
- (void)keyCommand_findPreviousStringInPage;
- (void)keyCommand_focusOmnibox;
- (void)keyCommand_closeTab;
- (void)keyCommand_showNextTab;
- (void)keyCommand_showPreviousTab;
- (void)keyCommand_showBookmarks;
- (void)keyCommand_addToBookmarks;
- (void)keyCommand_reload;
- (void)keyCommand_goBack;
- (void)keyCommand_goForward;
- (void)keyCommand_showHistory;
- (void)keyCommand_startVoiceSearch;
- (void)keyCommand_close;
- (void)keyCommand_showSettings;
- (void)keyCommand_stop;
- (void)keyCommand_showHelp;
- (void)keyCommand_showDownloadsFolder;
- (void)keyCommand_showTab0;
- (void)keyCommand_showTab1;
- (void)keyCommand_showTab2;
- (void)keyCommand_showTab3;
- (void)keyCommand_showTab4;
- (void)keyCommand_showTab5;
- (void)keyCommand_showTab6;
- (void)keyCommand_showTab7;
- (void)keyCommand_showLastTab;
- (void)keyCommand_reportAnIssue;
- (void)keyCommand_addToReadingList;
- (void)keyCommand_goToTabGrid;
- (void)keyCommand_clearBrowsingData;

@end

#endif  // IOS_CHROME_BROWSER_UI_KEYBOARD_KEY_COMMAND_ACTIONS_H_
