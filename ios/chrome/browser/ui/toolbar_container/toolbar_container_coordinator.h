// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_CONTAINER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_CONTAINER_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

@class ToolbarContainerViewController;

// Enum type describing which toolbars will be held by this container.
enum class ToolbarContainerType { kPrimary, kSecondary };

// Coordinator that manages a stack of toolbars.
@interface ToolbarContainerCoordinator : ChromeCoordinator

// Initializes a container with |type| and |browserState|.
- (instancetype)initWithBrowser:(Browser*)browser
                           type:(ToolbarContainerType)type
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The container view controller being managed by this coordinator.
@property(nonatomic, strong, readonly) UIViewController* viewController;

// The toolbar coordinators being managed by this container.
@property(nonatomic, strong) NSArray<ChromeCoordinator*>* toolbarCoordinators;

// Returns the height of the toolbars managed by this container at |progress|.
- (CGFloat)toolbarStackHeightForFullscreenProgress:(CGFloat)progress;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CONTAINER_TOOLBAR_CONTAINER_COORDINATOR_H_
