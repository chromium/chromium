// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class AppBarViewController;

// View controller for the App Bar container. This is the view controller in
// charge of making sure the app stays at the physical bottom of the screen.
// To do this, it is covering the whole screen and then manage the rotation
// itself. It needs to be centered in the window to work.
@interface AppBarContainerViewController : UIViewController

// Sets the App Bar view controller to be contained.
- (void)setAppBar:(AppBarViewController*)appBar;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_UI_APP_BAR_CONTAINER_VIEW_CONTROLLER_H_
