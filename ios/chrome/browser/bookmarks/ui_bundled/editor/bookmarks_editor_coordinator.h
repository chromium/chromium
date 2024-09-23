// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_H_
#define IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

namespace bookmarks {
class BookmarkNode;
}  // namespace bookmarks

@protocol SnackbarCommands;
@protocol BookmarksEditorCoordinatorDelegate;

// Coordinator to edit a bookmark based on an bookmark node or on an URL.
@interface BookmarksEditorCoordinator
    : ChromeCoordinator <UIAdaptivePresentationControllerDelegate>

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Initializes BookmarksEditorCoordinator. `node` has to be a valid bookmark
// node.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                      node:(const bookmarks::BookmarkNode*)node
                   snackbarCommandsHandler:
                       (id<SnackbarCommands>)snackbarCommandsHandler
    NS_DESIGNATED_INITIALIZER;

@property(nonatomic, weak) id<BookmarksEditorCoordinatorDelegate> delegate;

// Whether the dismissal is animated.
@property(nonatomic, assign) BOOL animatedDismissal;

// Whether the bookmark editor can be dismissed.
- (BOOL)canDismiss;

@end

#endif  // IOS_CHROME_BROWSER_BOOKMARKS_UI_BUNDLED_EDITOR_BOOKMARKS_EDITOR_COORDINATOR_H_
