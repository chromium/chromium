// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_COMPOSEBOX_EGTEST_UTIL_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_COMPOSEBOX_EGTEST_UTIL_H_

#import <Foundation/Foundation.h>

// Opens the tab picker from the composebox plus button menu, focusing the
// omnibox first if the composebox is not already visible.
void OpenComposeboxTabPicker();

// Selects a tab with the given `title` in the tab picker, scrolling if needed.
// Presupposes that the tab picker is already open.
void SelectTabInTabPickerWithTitle(NSString* title);

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_EG_TESTS_COMPOSEBOX_EGTEST_UTIL_H_
