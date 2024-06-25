// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_PRESENTATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_PRESENTATION_COMMANDS_H_

// Commands related to actions within the Quick Delete UI.
@protocol QuickDeletePresentationCommands

// Method invoked when the user wants to dismiss Quick Delete either via the
// 'Cancel' button or by dragging the view down.
- (void)dismissQuickDelete;

// Opens the provided My Activity `URL` in a new tap and dismissed the Quick
// Delete bottom sheet.
- (void)openMyActivityURL:(const GURL&)URL;

// Method invoked when the user taps the Browsing Data row to open a page to
// curate the list of selected browsing data to be deleted.
- (void)showBrowsingDataPage;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_PRESENTATION_COMMANDS_H_
