// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#include <set>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol BookmarksFolderChooserViewControllerPresentationDelegate;
class Browser;
@protocol SnackbarCommands;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

// A folder selector view controller.
// This controller monitors the state of the bookmark model, so changes to the
// bookmark model can affect this controller's state.
// The bookmark model is assumed to be loaded, thus also not to be NULL.
@interface BookmarksFolderChooserViewController : ChromeTableViewController

@property(nonatomic, weak)
    id<BookmarksFolderChooserViewControllerPresentationDelegate>
        delegate;
// Handler for Snackbar Commands.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;
// The current nodes (bookmarks or folders) that are considered for a move.
@property(nonatomic, assign, readonly)
    const std::set<const bookmarks::BookmarkNode*>& editedNodes;

// Initializes the view controller with a bookmarks model. `allowsNewFolders`
// will instruct the controller to provide the necessary UI to create a folder.
// `bookmarkModel` must not be NULL and must be loaded.
// `editedNodes` affects which cells can be selected, since it is not possible
// to move a node into its subnode.
// `allowsCancel` puts a cancel and done button in the navigation bar instead of
// a back button, which is needed if this view controller is presented modally.
- (instancetype)
    initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
         allowsNewFolders:(BOOL)allowsNewFolders
              editedNodes:(const std::set<const bookmarks::BookmarkNode*>&)nodes
             allowsCancel:(BOOL)allowsCancel
           selectedFolder:(const bookmarks::BookmarkNode*)selectedFolder
                  browser:(Browser*)browser;

// This method changes the currently selected folder and updates the UI. The
// delegate is not notified of the change.
- (void)changeSelectedFolder:(const bookmarks::BookmarkNode*)selectedFolder;
// TODO(crbug.com/1402758): Remove this method after model code is moved to the
// mediator.
// Notifies the view controller that a new `folder` was added.
- (void)notifyFolderNodeAdded:(const bookmarks::BookmarkNode*)folder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_CHOOSER_BOOKMARKS_FOLDER_CHOOSER_VIEW_CONTROLLER_H_
