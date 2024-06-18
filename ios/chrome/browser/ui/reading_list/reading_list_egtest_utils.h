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

// The cloud slash icon that appears for Reading List items that are only stored
// in the local storage. Shown only for signed-in users.
id<GREYMatcher> VisibleLocalItemIcon(NSString* title);

// Opens the reading list.
void OpenReadingList();

// Opens a URL and add it to the Reading List, without dismissing the snackbar.
void AddURLToReadingListWithoutSnackbarDismiss(const GURL& URL);

// Opens a URL and add it to the Reading List, and dismisses the snackbar.
// Needs to give `email` if the primary account is set.
void AddURLToReadingListWithSnackbarDismiss(const GURL& URL, NSString* email);

}  // namespace reading_list_test_utils

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_EGTEST_UTILS_H_
