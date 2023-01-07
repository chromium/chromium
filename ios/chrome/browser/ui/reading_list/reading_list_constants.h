// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_CONSTANTS_H_

#import <UIKit/UIKit.h>

// Accessibility identifier for reading list view.
extern NSString* const kReadingListViewID;

// Accessibility identifiers for reading list toolbar buttons.
extern NSString* const kReadingListToolbarEditButtonID;
extern NSString* const kReadingListToolbarDeleteButtonID;
extern NSString* const kReadingListToolbarDeleteAllReadButtonID;
extern NSString* const kReadingListToolbarCancelButtonID;
extern NSString* const kReadingListToolbarMarkButtonID;

// NSUserDefault key to save last time a Messages prompt was shown.
extern NSString* const kLastReadingListEntryAddedFromMessages;

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_CONSTANTS_H_
