// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COORDINATOR_DELEGATE_H_

@class ReadingListCoordinator;

// Delegate for ReadingListCoordinator.
@protocol ReadingListCoordinatorDelegate

// Called when the reading list should be dismissed.
- (void)closeReadingList;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_COORDINATOR_DELEGATE_H_
