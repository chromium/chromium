// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
class Browser;
@protocol BrowserCommands;
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
@interface BookmarkInteractionController : NSObject

// This object's delegate.
@property(nonatomic, weak) id<BookmarkInteractionControllerDelegate> delegate;

- (instancetype)initWithBrowser:(Browser*)browser
               parentController:(UIViewController*)parentController
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Presents the bookmark UI for a single bookmark with |webState|'s current
// committed URL and tab title.
- (void)presentBookmarkEditorForWebState:(web::WebState*)webState
                     currentlyBookmarked:(BOOL)bookmarked;

// Presents the bookmark UI for a single bookmark with |URL| and |title|.
- (void)presentBookmarkEditorForURL:(const GURL&)URL
                              title:(NSString*)title
                currentlyBookmarked:(BOOL)bookmarked;

// Presents the bookmarks browser modally.
- (void)presentBookmarks;

// Presents the bookmark or folder editor for the given |node|.
- (void)presentEditorForNode:(const bookmarks::BookmarkNode*)node;

// Removes any bookmark modal controller from view if visible.
// override this method.
- (void)dismissBookmarkModalControllerAnimated:(BOOL)animated;

// Removes any snackbar related to bookmarks that could have been presented.
- (void)dismissSnackbar;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_INTERACTION_CONTROLLER_H_
