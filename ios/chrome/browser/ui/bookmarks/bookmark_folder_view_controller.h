// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#include <set>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@class BookmarkFolderViewController;
@protocol BrowserCommands;
namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

@protocol BookmarkFolderViewControllerDelegate
// Called when a bookmark folder is selected. |folder| is the newly selected
// folder.
- (void)folderPicker:(BookmarkFolderViewController*)folderPicker
    didFinishWithFolder:(const bookmarks::BookmarkNode*)folder;
// Called when the user is done with the picker, either by tapping the Cancel or
// the Back button.
- (void)folderPickerDidCancel:(BookmarkFolderViewController*)folderPicker;
// Called when the user dismisses the picker by swiping down.
- (void)folderPickerDidDismiss:(BookmarkFolderViewController*)folderPicker;
@end

// A folder selector view controller.
//
// This controller monitors the state of the bookmark model, so changes to the
// bookmark model can affect this controller's state.
// The bookmark model is assumed to be loaded, thus also not to be NULL.
@interface BookmarkFolderViewController
    : ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

@property(nonatomic, weak) id<BookmarkFolderViewControllerDelegate> delegate;

// The current nodes (bookmarks or folders) that are considered for a move.
@property(nonatomic, assign, readonly)
    std::set<const bookmarks::BookmarkNode*>& editedNodes;

// Initializes the view controller with a bookmarks model. |allowsNewFolders|
// will instruct the controller to provide the necessary UI to create a folder.
// |bookmarkModel| must not be NULL and must be loaded.
// |editedNodes| affects which cells can be selected, since it is not possible
// to move a node into its subnode.
// |allowsCancel| puts a cancel and done button in the navigation bar instead of
// a back button, which is needed if this view controller is presented modally.
- (instancetype)
    initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
         allowsNewFolders:(BOOL)allowsNewFolders
              editedNodes:(const std::set<const bookmarks::BookmarkNode*>&)nodes
             allowsCancel:(BOOL)allowsCancel
           selectedFolder:(const bookmarks::BookmarkNode*)selectedFolder
               dispatcher:(id<BrowserCommands>)dispatcher;

// This method changes the currently selected folder and updates the UI. The
// delegate is not notified of the change.
- (void)changeSelectedFolder:(const bookmarks::BookmarkNode*)selectedFolder;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_VIEW_CONTROLLER_H_
