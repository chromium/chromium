// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDIT_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDIT_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/keyboard/key_command_actions.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@class BookmarkEditViewController;
@class BookmarkFolderViewController;
class Browser;
@protocol SnackbarCommands;

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

@protocol BookmarkEditViewControllerDelegate

// Called when the edited bookmark is set for deletion.
// If the delegate returns `YES`, all nodes matching the URL of `bookmark` will
// be deleted.
// If the delegate returns `NO`, only `bookmark` will be deleted.
// If the delegate doesn't implement this method, the default behavior is to
// delete all nodes matching the URL of `bookmark`.
- (BOOL)bookmarkEditor:(BookmarkEditViewController*)controller
    shoudDeleteAllOccurencesOfBookmark:(const bookmarks::BookmarkNode*)bookmark;

// Called when the controller should be dismissed.
- (void)bookmarkEditorWantsDismissal:(BookmarkEditViewController*)controller;

// Called when the controller is going to commit the title or URL change.
- (void)bookmarkEditorWillCommitTitleOrUrlChange:
    (BookmarkEditViewController*)controller;

@end

// View controller for editing bookmarks. Allows editing of the title, URL and
// the parent folder of the bookmark.
//
// This view controller will also monitor bookmark model change events and react
// accordingly depending on whether the bookmark and folder it is editing
// changes underneath it.
@interface BookmarkEditViewController
    : ChromeTableViewController <KeyCommandActions,
                                 UIAdaptivePresentationControllerDelegate>

@property(nonatomic, weak) id<BookmarkEditViewControllerDelegate> delegate;

// Snackbar commands handler.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Designated initializer.
// `bookmark`: mustn't be NULL at initialization time. It also mustn't be a
//             folder.
- (instancetype)initWithBookmark:(const bookmarks::BookmarkNode*)bookmark
                         browser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

// Called before the instance is deallocated.
- (void)shutdown;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDIT_VIEW_CONTROLLER_H_
