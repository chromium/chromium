// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SNACKBAR_SNACKBAR_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SNACKBAR_SNACKBAR_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

@class SnackbarCoordinator;

// A delegate which provides an offset from the bottom of the window to
// present a snackbar message above.
@protocol SnackbarCoordinatorDelegate <NSObject>

// Returns the current offset to use from the bottom of the screen to display
// the snackbar UI. When `forceBrowserToolbar`, uses the browser's toolbar
// height, ignoring presented view controllers.
- (CGFloat)snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:
               (SnackbarCoordinator*)snackbarCoordinator
                                                forceBrowserToolbar:
                                                    (BOOL)forceBrowserToolbar;

@end

#endif  // IOS_CHROME_BROWSER_UI_SNACKBAR_SNACKBAR_COORDINATOR_DELEGATE_H_
