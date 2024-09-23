// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/bookmarks/ui_bundled/folder_editor/bookmarks_folder_editor_coordinator_delegate.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Coordinator to edit a bookmark folder based on an bookmark node, or create
// a new folder.
@interface BookmarksFolderEditorCoordinator : ChromeCoordinator

// Coordinator's delegate.
@property(nonatomic, weak) id<BookmarksFolderEditorCoordinatorDelegate>
    delegate;

// Initializes BookmarksFolderEditorCoordinator, to create a new folder in
// `parentFolderNode`.
// `parentFolderNode` cannot be `nullptr`.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
                                parentFolderNode:
                                    (const bookmarks::BookmarkNode*)parentFolder
    NS_DESIGNATED_INITIALIZER;
// Initializes BookmarksFolderEditorCoordinator. The view will edit the bookmark
// folder node.
// `folderNode` cannot be `nullptr` and must be editable (`folderNode` can't be
// the root node or any of the permanent nodes).
- (instancetype)
    initWithBaseViewController:(UIViewController*)navigationController
                       browser:(Browser*)browser
                    folderNode:(const bookmarks::BookmarkNode*)folder
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Whether the bookmark folder editor can be dismissed.
- (BOOL)canDismiss;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_COORDINATOR_H_
