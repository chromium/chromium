// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/commands/bookmarks_commands.h"

class Browser;
@protocol BookmarkInteractionControllerDelegate;

namespace bookmarks {
class BookmarkNode;
}

class GURL;

namespace web {
class WebState;
}

// The BookmarkInteractionController abstracts the management of the various
// UIViewControllers used to create, remove and edit a bookmark.
@interface BookmarkInteractionController : NSObject <BookmarksCommands>

// This object's delegate.
@property(nonatomic, weak) id<BookmarkInteractionControllerDelegate> delegate;

// The parent controller on top of which the UI needs to be presented.
@property(nonatomic, weak) UIViewController* parentController;

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Called before the instance is deallocated.
- (void)shutdown;

// Adds a bookmark for `URL` with the given `title`.
- (void)bookmarkURL:(const GURL&)URL title:(NSString*)title;

// Presents the bookmark UI to edit an existing bookmark with `URL`.
- (void)presentBookmarkEditorForURL:(const GURL&)URL;

// Presents the bookmarks browser modally.
- (void)presentBookmarks;

// Presents the bookmark or folder editor for the given `node`.
- (void)presentEditorForNode:(const bookmarks::BookmarkNode*)node;

// Removes any bookmark modal controller from view if visible.
// override this method.
- (void)dismissBookmarkModalControllerAnimated:(BOOL)animated;

// Removes any snackbar related to bookmarks that could have been presented.
- (void)dismissSnackbar;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_H_
