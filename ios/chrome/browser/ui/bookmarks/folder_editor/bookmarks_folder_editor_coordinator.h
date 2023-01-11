// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/bookmarks/folder_editor/bookmarks_folder_editor_coordinator_delegate.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Coordinator to edit a bookmark folder based on an bookmark node, or create
// a new folder.
@interface BookmarksFolderEditorCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;
// Initializes BookmarksFolderEditorCoordinator, to create a new folder.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;
// Initializes BookmarksFolderEditorCoordinator. The view will edit the bookmark
// folder node. `folderNode` cannot be nil.
- (instancetype)
    initWithBaseViewController:(UIViewController*)navigationController
                       browser:(Browser*)browser
                    folderNode:(const bookmarks::BookmarkNode*)folderNode
    NS_DESIGNATED_INITIALIZER;

// Coordinator's delegate.
@property(nonatomic, weak) id<BookmarksFolderEditorCoordinatorDelegate>
    delegate;
// Returns the edited folder node. When creating a new folder, the new folder
// node can be retrieved after `-[id<BookmarksFolderEditorCoordinatorDelegate>
// bookmarksFolderEditorCoordinatorShouldStop:]` is called.
@property(nonatomic, assign, readonly)
    const bookmarks::BookmarkNode* editedFolderNode;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_H_
