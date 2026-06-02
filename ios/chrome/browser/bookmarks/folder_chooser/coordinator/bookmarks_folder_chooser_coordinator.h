// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import <set>

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/bookmarks/folder_chooser/coordinator/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

@class TableViewNavigationController;

// Coordinator to select a bookmark folder. This view lists the bookmark
// folder hierarchy, to let the user chooser a folder.
@interface BookmarksFolderChooserCoordinator : ChromeCoordinator

// Coordinator's delegate.
@property(nonatomic, weak) id<BookmarksFolderChooserCoordinatorDelegate>
    delegate;
// Will provide the necessary UI to create a folder. `YES` by default.
// Should be set before calling `start`.
@property(nonatomic) BOOL allowsNewFolders;

// Initializes BookmarksFolderChooserCoordinator. The view is pushed into
// `navigationController`.
// `selectedFolder` will be the folder with check mark in the UI.
// `movedNodes` is a list of nodes that are currently being moved, they must
// thus be hidden in the chooser view. This is to make sure a folder cannot be
// moved into one of its children.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                          movedNodes:
                              (const std::set<
                                  raw_ptr<const bookmarks::BookmarkNode>>&)
                                  movedNodes;

// Initializes BookmarksFolderChooserCoordinator. A navigation controller is
// created, with the chooser folder view as the root view controller.
// `selectedFolder` will be the folder with check mark in the UI.
// `movedNodes` is a list of nodes that are currently being moved, they must
// thus be hidden in the chooser view. This is to make sure a folder cannot be
// moved into one of its children.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                    movedNodes:
                        (const std::set<
                            raw_ptr<const bookmarks::BookmarkNode>>&)movedNodes
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Whether the bookmark folder chooser can be dismissed.
- (BOOL)canDismiss;
// The value of `movedNodes` during init, minus the nodes that have been
// deleted in the meantime.
- (const std::set<raw_ptr<const bookmarks::BookmarkNode>>&)movedNodes;
// Puts a blue check mark beside a folder it in the UI.
// If unset no blue check mark is shown.
- (void)setSelectedFolder:(const bookmarks::BookmarkNode*)folder;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_H_
