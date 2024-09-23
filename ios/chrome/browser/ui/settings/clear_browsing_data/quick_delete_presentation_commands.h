// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_PRESENTATION_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_PRESENTATION_COMMANDS_H_

#import "ios/chrome/browser/browsing_data/model/tabs_closure_util.h"

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

// Trigger the tabs closure animation along with the actual closure of the
// WebStates within the deletion time frame. It also indicates if reloading tabs
// is necessary after the deletion has finished.
- (void)triggerTabsClosureAnimationWithBeginTime:(base::Time)beginTime
                                         endTime:(base::Time)endTime
                                  cachedTabsInfo:
                                      (tabs_closure_util::WebStateIDToTime)
                                          cachedTabsInfo
                            forceWebStatesReload:(BOOL)forceWebStatesReload;

// Method invoked on deletion in progress to block other windows to avoid having
// multiple deletions occur concurrently.
- (void)blockOtherWindows;

// Method invoked on deletion completed to release other blocked windows.
- (void)releaseOtherWindows;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_CLEAR_BROWSING_DATA_QUICK_DELETE_PRESENTATION_COMMANDS_H_
