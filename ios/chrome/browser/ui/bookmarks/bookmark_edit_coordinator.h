// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDIT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDIT_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"
#include "url/gurl.h"

@class BookmarkEditCoordinator;

namespace bookmarks {
class BookmarkNode;
}

// Delegate protocol for getting updates from the coordinator.
@protocol BookmarkEditCoordinatorDelegate

// Called when the bookmark edit view has been dismissed. The |coordinator|
// passes itself as parameter.
- (void)bookmarkEditDismissed:(BookmarkEditCoordinator*)coordinator;

@end

// BookmarkEditCoordinator presents the public interface for a slimmed-down
// bookmark edit functionality.
@interface BookmarkEditCoordinator : ChromeCoordinator

// Initializes an instance with a base |viewController|, the current |browser|,
// and the |bookmark| to edit.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  bookmark:
                                      (const bookmarks::BookmarkNode*)bookmark
    NS_DESIGNATED_INITIALIZER;

// Unavailable, use -initWithBaseViewController:browser:title:URL:.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@property(nonatomic, weak) id<BookmarkEditCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_EDIT_COORDINATOR_H_
