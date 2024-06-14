// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_DELEGATE_H_

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

@class BookmarksFolderEditorCoordinator;

// Delegate for BookmarksFolderEditorCoordinator.
@protocol BookmarksFolderEditorCoordinatorDelegate <NSObject>

// Called when the coordinator successfully created or edited `folder`.
- (void)bookmarksFolderEditorCoordinator:
            (BookmarksFolderEditorCoordinator*)folderEditor
              didFinishEditingFolderNode:(const bookmarks::BookmarkNode*)folder;
// Called when the user deletes the folder or cancels folder edition.
- (void)bookmarksFolderEditorCoordinatorShouldStop:
    (BookmarksFolderEditorCoordinator*)coordinator;
// Called when the coordinator is going to commit the title change.
- (void)bookmarksFolderEditorWillCommitTitleChange:
    (BookmarksFolderEditorCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_DELEGATE_H_
