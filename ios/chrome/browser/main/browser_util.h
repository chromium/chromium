// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_UTIL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_UTIL_H_

#import <Foundation/Foundation.h>

#import <set>

#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"

class Browser;
namespace web {
class WebStateID;
}  // namespace web

// Stores information about a tab.
struct BrowserAndIndex {
  Browser* browser = nullptr;
  int tab_index = WebStateList::kInvalidIndex;
};

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
void MoveTabToBrowser(web::WebStateID tab_id,
                      Browser* destination_browser,
                      int destination_tab_index,
                      WebStateList::InsertionFlags flags);

// Moves the tab to the `destination_tab_index` in `destination_browser`. It is
// an error to try to move a tab across profiles (incognito <-> regular).
void MoveTabToBrowser(web::WebStateID tab_id,
                      Browser* destination_browser,
                      int destination_tab_index);

// Given a set of `browsers`, finds the one with `tab_id`. Returns a
// BrowserAndIndex that contains the browser and `tab_index` of the tab.
BrowserAndIndex FindBrowserAndIndex(web::WebStateID tab_id,
                                    const std::set<Browser*>& browsers);

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_UTIL_H_
