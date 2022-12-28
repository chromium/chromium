// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_DELEGATE_H_

@class BookmarksEditorCoordinator;

// Delegate for BookmarksEditorCoordinator.
@protocol BookmarksEditorCoordinatorDelegate <NSObject>

// Called when the coordinator needs to be stopped.
- (void)bookmarksEditorCoordinatorShouldStop:
    (BookmarksEditorCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_DELEGATE_H_
