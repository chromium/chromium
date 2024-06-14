// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/bookmarks_commands.h"

class Browser;
@protocol BookmarksCoordinatorDelegate;

namespace bookmarks {
class BookmarkNode;
}

class GURL;

// The BookmarksCoordinator abstracts the management of the various
// pieces of UI used to create, remove and edit a bookmark.
@interface BookmarksCoordinator : ChromeCoordinator <BookmarksCommands>

// This object's delegate.
@property(nonatomic, weak) id<BookmarksCoordinatorDelegate> delegate;

// The base view controller for this coordinator
@property(nonatomic, weak, readwrite) UIViewController* baseViewController;

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Adds a bookmark for `URL` with the given `title`.
- (void)createBookmarkURL:(const GURL&)URL title:(NSString*)title;

// Presents the bookmark UI to edit an existing bookmark with `URL`.
- (void)presentBookmarkEditorForURL:(const GURL&)URL;

// Presents the bookmarks browser modally.
- (void)presentBookmarks;

// Presents the folder editor for the given folder `node`.
- (void)presentEditorForFolderNode:(const bookmarks::BookmarkNode*)node;

// Presents the bookmark editor for the given URL `node`.
- (void)presentEditorForURLNode:(const bookmarks::BookmarkNode*)node;

// Removes any bookmark modal controller from view if visible.
// override this method.
- (void)dismissBookmarkModalControllerAnimated:(BOOL)animated;

// Removes any snackbar related to bookmarks that could have been presented.
- (void)dismissSnackbar;

// whether the current bookmark view can be dismissed.
- (BOOL)canDismiss;

// Presents the signed-in account settings page.
- (void)showAccountSettings;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_HOME_BOOKMARKS_COORDINATOR_H_
