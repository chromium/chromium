// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/chrome_root_coordinator.h"
#import "ios/chrome/browser/ui/tab_grid/view_controller_swapping.h"

@protocol ApplicationCommands;
@class TabModel;
@protocol TabSwitcher;

@interface TabGridCoordinator : ChromeRootCoordinator<ViewControllerSwapping>

- (instancetype)initWithWindow:(UIWindow*)window
    applicationCommandEndpoint:
        (id<ApplicationCommands>)applicationCommandEndpoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithWindow:(UIWindow*)window NS_UNAVAILABLE;

@property(nonatomic, readonly) id<TabSwitcher> tabSwitcher;

@property(nonatomic, weak) TabModel* regularTabModel;
@property(nonatomic, weak) TabModel* incognitoTabModel;

// If this property is YES, calls to |showTabSwitcher:completion:| and
// |showTabViewController:completion:| will present the given view controllers
// without animation.  This should only be used by unittests.
@property(nonatomic, readwrite, assign) BOOL animationsDisabledForTesting;

// Stops all child coordinators then calls |completion|. |completion| is called
// whether or not child coordinators exist.
- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
