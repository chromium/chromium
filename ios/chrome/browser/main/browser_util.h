// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MAIN_BROWSER_UTIL_H_
#define IOS_CHROME_BROWSER_MAIN_BROWSER_UTIL_H_

#import <Foundation/Foundation.h>

class Browser;

// Moves the tab to the `destination_tab_index` in `destination_browser`. It is
// an error to try to move a tab across profiles (incognito <-> regular).
void MoveTabToBrowser(NSString* tab_id,
                      Browser* destination_browser,
                      int destination_tab_index);

#endif  // IOS_CHROME_BROWSER_MAIN_BROWSER_UTIL_H_
