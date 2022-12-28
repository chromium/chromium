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

// Coordinator to select a bookmark folder. This view lists the bookmark
// folder hierarchy, to let the user chooser a folder.
@interface BookmarksFolderChooserCoordinator : ChromeCoordinator

// Initializes BookmarksFolderChooserCoordinator. The view is pushed into
// `navigationController`.
// The view will hide `hiddenNodes`. This is to make sure a folder cannot be
// moved into one of its children.
- (instancetype)
    initWithBaseNavigationController:
        (UINavigationController*)navigationController
                             browser:(Browser*)browser
                         hiddenNodes:
                             (const std::set<const bookmarks::BookmarkNode*>&)
                                 hiddenNodes NS_DESIGNATED_INITIALIZER;

// Initializes BookmarksFolderChooserCoordinator. A navigation controller is
// created, with the chooser folder view as the root view controller.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

// Coordinator's delegate.
@property(nonatomic, weak) id<BookmarksFolderChooserCoordinatorDelegate>
    delegate;
// Is `nil` if the folder is not yet selected, or if the user canceled the
// dialog. The value is set just before
// `bookmarksFolderChooserCoordinatorShouldStop:` is called.
@property(nonatomic, assign, readonly)
    const bookmarks::BookmarkNode* selectedFolder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_COORDINATOR_H_
