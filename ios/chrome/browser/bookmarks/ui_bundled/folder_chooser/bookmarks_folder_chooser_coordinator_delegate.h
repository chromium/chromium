// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_DELEGATE_H_

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

@class BookmarksFolderChooserCoordinator;

// Delegate for BookmarksFolderChooserCoordinator.
@protocol BookmarksFolderChooserCoordinatorDelegate <NSObject>

// Called when the user confirms a folder selection.
- (void)bookmarksFolderChooserCoordinatorDidConfirm:
            (BookmarksFolderChooserCoordinator*)coordinator
                                 withSelectedFolder:
                                     (const bookmarks::BookmarkNode*)folder;

// Called when the user cancels or dismisses (by swiping down) the folder
// selection.
- (void)bookmarksFolderChooserCoordinatorDidCancel:
    (BookmarksFolderChooserCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_DELEGATE_H_
