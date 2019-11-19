// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_EDITOR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_EDITOR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@class BookmarkFolderEditorViewController;
@protocol BrowserCommands;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
}  // namespace bookmarks

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@protocol BookmarkFolderEditorViewControllerDelegate
// Called when the controller successfully created or edited |folder|.
- (void)bookmarkFolderEditor:(BookmarkFolderEditorViewController*)folderEditor
      didFinishEditingFolder:(const bookmarks::BookmarkNode*)folder;
// Called when the user deletes the edited folder.
// This is never called if the editor is created with
// |folderCreatorWithBookmarkModel:parentFolder:|.
- (void)bookmarkFolderEditorDidDeleteEditedFolder:
    (BookmarkFolderEditorViewController*)folderEditor;
// Called when the user cancels the folder creation.
- (void)bookmarkFolderEditorDidCancel:
    (BookmarkFolderEditorViewController*)folderEditor;
// Called when the controller is going to commit the title change.
- (void)bookmarkFolderEditorWillCommitTitleChange:
    (BookmarkFolderEditorViewController*)folderEditor;
@end

// View controller for creating or editing a bookmark folder. Allows editing of
// the title and selecting the parent folder of the bookmark.
// This controller monitors the state of the bookmark model, so changes to the
// bookmark model can affect this controller's state.
@interface BookmarkFolderEditorViewController
    : ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

@property(nonatomic, weak) id<BookmarkFolderEditorViewControllerDelegate>
    delegate;

// Designated factory methods.

// Returns a view controller set to create a new folder in |parentFolder|.
// If |parentFolder| is NULL, a default parent will be set.
// |bookmarkModel| must not be NULL and must be loaded.
+ (instancetype)
    folderCreatorWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                      parentFolder:(const bookmarks::BookmarkNode*)parentFolder
                        dispatcher:(id<BrowserCommands>)dispatcher;

// |bookmarkModel| must not be null and must be loaded.
// |folder| must not be NULL and be editable.
// |browserState| must not be null.
+ (instancetype)
    folderEditorWithBookmarkModel:(bookmarks::BookmarkModel*)bookmarkModel
                           folder:(const bookmarks::BookmarkNode*)folder
                     browserState:(ios::ChromeBrowserState*)browserState
                       dispatcher:(id<BrowserCommands>)dispatcher;

- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:(ChromeTableViewControllerStyle)style
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_FOLDER_EDITOR_VIEW_CONTROLLER_H_
