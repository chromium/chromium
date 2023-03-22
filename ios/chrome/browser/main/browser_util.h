// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_UTIL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_UTIL_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/web_state_list/web_state_list.h"

class Browser;

// Move the web state from `source_browser` at `source_tab_index` to
// `destination_browser` web state list at `destination_tab_index` with the
// given flag.
void MoveTabFromBrowserToBrowser(Browser* source_browser,
                                 int source_tab_index,
                                 Browser* destination_browser,
                                 int destination_tab_index,
                                 WebStateList::InsertionFlags flags);

// Move the web state from `source_browser` at `source_tab_index` to
// `destination_browser` web state list at `destination_tab_index`.
void MoveTabFromBrowserToBrowser(Browser* source_browser,
                                 int source_tab_index,
                                 Browser* destination_browser,
                                 int destination_tab_index);

// Moves the tab to the `destination_tab_index` in `destination_browser` with
// the given flag. It is an error to try to move a tab across profiles
// (incognito <-> regular).
void MoveTabToBrowser(NSString* tab_id,
                      Browser* destination_browser,
                      int destination_tab_index,
                      WebStateList::InsertionFlags flags);

// Moves the tab to the `destination_tab_index` in `destination_browser`. It is
// an error to try to move a tab across profiles (incognito <-> regular).
void MoveTabToBrowser(NSString* tab_id,
                      Browser* destination_browser,
                      int destination_tab_index);

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_UTIL_H_
