// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/bookmarks/editor/bookmarks_editor_coordinator_delegate.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

class GURL;
namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

// Coordinator to edit a bookmark based on an bookmark node or on an URL.
@interface BookmarksEditorCoordinator : ChromeCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Initializes BookmarksEditorCoordinator. `node` has to be a valid bookmark
// node.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      node:(const bookmarks::BookmarkNode*)node
    NS_DESIGNATED_INITIALIZER;
// Initializes BookmarksEditorCoordinator. Edits the first bookmark node
// pointing to `URL`.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                       URL:(const GURL&)URL
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic, weak) id<BookmarksEditorCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_H_
