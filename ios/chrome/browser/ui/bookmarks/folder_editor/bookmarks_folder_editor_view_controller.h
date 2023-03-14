// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import <set>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"

@class BookmarksFolderEditorViewController;
class Browser;
@protocol SnackbarCommands;
class SyncSetupService;

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
    : ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

@property(nonatomic, weak) id<BookmarksFolderEditorViewControllerDelegate>
    delegate;

// Snackbar commands handler for this ViewController.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Designated factory methods.

// Returns a view controller set to create a new folder in `parentFolder`.
// If `parentFolder` is NULL, a default parent will be set.
// `bookmarkModel` must not be NULL and must be loaded.
+ (instancetype)
    folderCreatorWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                      parentFolder:(const bookmarks::BookmarkNode*)parentFolder
                           browser:(Browser*)browser
                  syncSetupService:(SyncSetupService*)syncSetupService
                       syncService:(syncer::SyncService*)syncService;

// `bookmarkModel` must not be null and must be loaded.
// `folder` must not be NULL and be editable.
// `browser` must not be null.
+ (instancetype)
    folderEditorWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                           folder:(const bookmarks::BookmarkNode*)folder
                          browser:(Browser*)browser
                 syncSetupService:(SyncSetupService*)syncSetupService
                      syncService:(syncer::SyncService*)syncService;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Called when the user attempt to swipe down the view controller.
- (void)presentationControllerDidAttemptToDismiss;
// Whether the bookmarks folder editor can be dismissed.
- (BOOL)canDismiss;
// TODO(crbug.com/1402758): Remove this method after model code is moved to the
// mediator.
- (void)updateParentFolder:(const bookmarks::BookmarkNode*)parent;
// Stops listening to update to the bookmarks model and the sync model
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_FOLDER_EDITOR_BOOKMARKS_FOLDER_EDITOR_VIEW_CONTROLLER_H_
