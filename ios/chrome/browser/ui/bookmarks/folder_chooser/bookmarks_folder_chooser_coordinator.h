// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_H_

#import <UIKit/UIKit.h>
#import <set>

#import "ios/chrome/browser/ui/bookmarks/folder_chooser/bookmarks_folder_chooser_coordinator_delegate.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

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
// The current nodes (bookmarks or folders) that are considered for a move.
// Will be set right before this coordinator sends a confirm selection signal
// through it's delegate.
@property(nonatomic, assign, readonly)
    const std::set<const bookmarks::BookmarkNode*>& editedNodes;
// The folder that has a blue check mark beside it in the UI. This property has
// 2 functionality.
// - It can be set while initializing this coordinator to already have a blue
//   check mark beside the folder in the UI. If unset no check mark is shown.
// - This property will also hold the folder the user selected. This
//   information should be accessed when the `delegate` sends a confirmation
//   of folder selection.
@property(nonatomic, assign) const bookmarks::BookmarkNode* selectedFolder;
// Will provide the necessary UI to create a folder. `YES` by default.
// Should be set before calling `start`.
@property(nonatomic) BOOL allowsNewFolders;

// Initializes BookmarksFolderChooserCoordinator. The view is pushed into
// `navigationController`.
// `selectedFolder` will be the folder with check mark in the UI.
// `hiddenNodes` is a list of nodes to hide in the chooser view. This is to
// make sure a folder cannot be moved into one of its children.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                         hiddenNodes:
                             (const std::set<const bookmarks::BookmarkNode*>&)
                                 hiddenNodes;
// Initializes BookmarksFolderChooserCoordinator. A navigation controller is
// created, with the chooser folder view as the root view controller.
// `selectedFolder` will be the folder with check mark in the UI.
// `hiddenNodes` is a list of nodes to hide in the chooser view. This is to
// make sure a folder cannot be moved into one of its children.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   hiddenNodes:(const std::set<const bookmarks::BookmarkNode*>&)
                                   hiddenNodes NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Whether the bookmark folder chooser can be dismissed.
- (BOOL)canDismiss;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_H_
