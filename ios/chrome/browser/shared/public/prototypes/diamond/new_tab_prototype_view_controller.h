// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_NEW_TAB_PROTOTYPE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_NEW_TAB_PROTOTYPE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

class Browser;

// View Controller containing the omnibox for the prototype, to be displayed in
// a sheet.
@interface NewTabPrototypeViewController : UIViewController

// `isNewTabPage` is true if when the user navigate using the omnibox, the
// result should be opened in a new tab, false if it should be opened in the
// active tab. `shouldExitTabGrid` is whether the tab grid should be exited when
// the user is choosing an action.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              isNewTabPage:(BOOL)isNewTabPage
                         shouldExitTabGrid:(BOOL)shouldExitTabGrid;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_PROTOTYPES_DIAMOND_NEW_TAB_PROTOTYPE_VIEW_CONTROLLER_H_
