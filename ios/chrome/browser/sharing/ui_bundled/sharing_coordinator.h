// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_SHARING_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_SHARING_COORDINATOR_H_

#include <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol ActivityServiceCommands;
@class SharingParams;
class Browser;

// Coordinator of sharing scenarios. Its default scenario is to share the
// current tab's URL.
@interface SharingCoordinator : ChromeCoordinator

// Creates a coordinator configured to share the current tab's URL using the
// base `viewController`, a `browser`, `params` with all the necessary values
// to drive the scenario. `sourceItem` is used to position the share menu.
- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        params:(SharingParams*)params
                    sourceItem:(id<UIPopoverPresentationControllerSourceItem>)
                                   sourceItem NS_DESIGNATED_INITIALIZER;

// Same as above, but use `sourceView` and `sourceRect` to position the menu.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                    params:(SharingParams*)params
                                sourceView:(UIView*)sourceView
                                sourceRect:(CGRect)sourceRect
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// If there is a download currently happening, this cancels it and triggers a
// new coordinator to be created.
- (void)cancelIfNecessaryAndCreateNewCoordinatorFromView:(UIView*)shareButton;

@end

#endif  // IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_SHARING_COORDINATOR_H_
