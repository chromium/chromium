// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_CONTROLLER_AUDIENCE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_CONTROLLER_AUDIENCE_H_

// Audience for the Reading List view controllers.
@protocol ReadingListListViewControllerAudience

// Whether the collection has items.
- (void)readingListHasItems:(BOOL)hasItems;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_LIST_VIEW_CONTROLLER_AUDIENCE_H_
