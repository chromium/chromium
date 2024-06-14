// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_COORDINATOR_DELEGATE_H_

@class BookmarksCoordinator;

// BookmarksCoordinatorDelegate provides methods for the coordinator to
// notify its delegate when certain events occur.
@protocol BookmarksCoordinatorDelegate

// Called when the coordinator is going to commit the title or URL change.
- (void)bookmarksCoordinatorWillCommitTitleOrURLChange:
    (BookmarksCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_COORDINATOR_DELEGATE_H_
