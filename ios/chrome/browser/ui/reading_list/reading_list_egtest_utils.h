// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_EGTEST_UTILS_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_EGTEST_UTILS_H_

#import <Foundation/Foundation.h>

class GURL;
@protocol GREYMatcher;

namespace reading_list_test_utils {

// Matcher for the snackbar that appears after an item is added to local Reading
// List storage.
id<GREYMatcher> AddedToLocalReadingListSnackbar();

// Matcher for the Reading List item cell with a given title.
id<GREYMatcher> ReadingListItem(NSString* entryTitle);

// Matcher for the currently visible Reading List item cell with a given title.
id<GREYMatcher> VisibleReadingListItem(NSString* entryTitle);

// Opens the reading list.
void OpenReadingList();

// Opens a URL and add it to the Reading List.
void AddURLToReadingList(const GURL& URL);

}  // namespace reading_list_test_utils

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_EGTEST_UTILS_H_
