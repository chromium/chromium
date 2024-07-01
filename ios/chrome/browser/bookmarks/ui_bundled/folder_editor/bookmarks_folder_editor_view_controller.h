// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import <set>

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller.h"

class AuthenticationService;
@class BookmarksFolderEditorViewController;
class Browser;
@protocol SnackbarCommands;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace syncer {
class SyncService;
}  // namespace syncer

@protocol BookmarksFolderEditorViewControllerDelegate

// Used to show the folder chooser UI when the user decides to update the
// parent folder.
- (void)showBookmarksFolderChooserWithParentFolder:
            (const bookmarks::BookmarkNode*)parent
                                       hiddenNodes:
                                           (const std::set<
                                               const bookmarks::BookmarkNode*>&)
                                               hiddenNodes;
// Called when the controller successfully created or edited `folder`.
- (void)bookmarksFolderEditor:(BookmarksFolderEditorViewController*)folderEditor
       didFinishEditingFolder:(const bookmarks::BookmarkNode*)folder;
// Called when the user deletes the edited folder.
// This is never called if the editor is created with
// `folderCreatorWithBookmarkModel:parentFolder:`.
- (void)bookmarksFolderEditorDidDeleteEditedFolder:
    (BookmarksFolderEditorViewController*)folderEditor;
// Called when the user cancels the folder creation.
- (void)bookmarksFolderEditorDidCancel:
    (BookmarksFolderEditorViewController*)folderEditor;
// Called when the view controller disappears either through
// 1. swiping right.
// 2. or pressing the back button when cancel button is not available.
- (void)bookmarksFolderEditorDidDismiss:
    (BookmarksFolderEditorViewController*)folderEditor;
// Called when the controller is going to commit the title change.
- (void)bookmarksFolderEditorWillCommitTitleChange:
    (BookmarksFolderEditorViewController*)folderEditor;

@end

// View controller for creating or editing a bookmark folder. Allows editing of
// the title and selecting the parent folder of the bookmark.
// This controller monitors the state of the bookmark model, so changes to the
// bookmark model can affect this controller's state.
@interface BookmarksFolderEditorViewController
    : LegacyChromeTableViewController <UIAdaptivePresentationControllerDelegate>

@property(nonatomic, weak) id<BookmarksFolderEditorViewControllerDelegate>
    delegate;

// Snackbar commands handler for this ViewController.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// `bookmarkModel` must not be `nullptr` and must be loaded.
// `parentFolder` must not be `nullptr`.
// If `folder` is not `nullptr` than it means we're editing an existing folder
// and `folder` must also be editable (`folder` can't be the root node or any
// of the permanent nodes).
// `browser` must not be `nullptr`.
- (instancetype)initWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                           folderNode:(const bookmarks::BookmarkNode*)folder
                     parentFolderNode:
                         (const bookmarks::BookmarkNode*)parentFolder
                authenticationService:(AuthenticationService*)authService
                          syncService:(syncer::SyncService*)syncService
                              browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Called when the user attempt to swipe down the view controller.
- (void)presentationControllerDidAttemptToDismiss;
// Whether the bookmarks folder editor can be dismissed.
- (BOOL)canDismiss;
// TODO(crbug.com/40251259): Remove this method after model code is moved to the
// mediator.
- (void)updateParentFolder:(const bookmarks::BookmarkNode*)parent;
// Stops listening to update to the bookmarks model and the sync model
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_VIEW_CONTROLLER_H_
