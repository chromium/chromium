// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <set>
#include <vector>

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class BookmarkHomeViewController;
namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks
class GURL;
namespace ios {
class ChromeBrowserState;
}  // namespace ios
class WebStateList;

@protocol BookmarkHomeViewControllerDelegate
// The view controller wants to be dismissed. If |urls| is not empty, then
// the user has selected to navigate to those URLs in the current tab mode.
- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarkHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls;

// The view controller wants to be dismissed. If |urls| is not empty, then
// the user has selected to navigate to those URLs with specified tab mode.
- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarkHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls
                                     inIncognito:(BOOL)inIncognito
                                          newTab:(BOOL)newTab;

@end

// Class to navigate the bookmark hierarchy.
@interface BookmarkHomeViewController
    : ChromeTableViewController <UIAdaptivePresentationControllerDelegate>

// Delegate for presenters. Note that this delegate is currently being set only
// in case of handset, and not tablet. In the future it will be used by both
// cases.
@property(nonatomic, weak) id<BookmarkHomeViewControllerDelegate> homeDelegate;

// Initializers.
- (instancetype)
    initWithBrowserState:(ios::ChromeBrowserState*)browserState
              dispatcher:(id<ApplicationCommands, BrowserCommands>)dispatcher
            webStateList:(WebStateList*)webStateList NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)tableViewStyle
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;

// Setter to set _rootNode value.
- (void)setRootNode:(const bookmarks::BookmarkNode*)rootNode;

// Returns an array of BookmarkHomeViewControllers, one per BookmarkNode in the
// path from this view controller's node to the latest cached node (as
// determined by BookmarkPathCache).  Includes |self| as the first element of
// the returned array.  Sets the cached scroll position for the last element of
// the returned array, if appropriate.
- (NSArray<BookmarkHomeViewController*>*)cachedViewControllerStack;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_HOME_VIEW_CONTROLLER_H_
