// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_BROWSER_CONTENT_COORDINATOR_H_
#define IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_BROWSER_CONTENT_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class BrowserContentViewController;
@protocol EditMenuBuilder;

// A coordinator that creates a container UIViewController that displays the
// web contents of the browser view.
@interface BrowserContentCoordinator : ChromeCoordinator

// The view controller managing the container view.
@property(nonatomic, strong, readonly)
    BrowserContentViewController* viewController;

// The builder for the edit menu.
- (id<EditMenuBuilder>)editMenuBuilder;

@end

#endif  // IOS_CHROME_BROWSER_BROWSER_CONTENT_UI_BUNDLED_BROWSER_CONTENT_COORDINATOR_H_
