// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_DELEGATE_H_

@class BookmarksFolderEditorCoordinator;

// Delegate for BookmarksFolderEditorCoordinator.
@protocol BookmarksFolderEditorCoordinatorDelegate <NSObject>

// Called when the coordinator needs to be stopped.
- (void)bookmarksFolderEditorCoordinatorShouldStop:
    (BookmarksFolderEditorCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_DELEGATE_H_
