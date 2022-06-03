// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_HIDER_BROWSER_VIEW_HIDER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_HIDER_BROWSER_VIEW_HIDER_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

class LocationBarModel;
@protocol ViewRevealingAnimatee;
@class ViewRevealingVerticalPanHandler;

// Coordinator for a view that hides the browser view controller's view.
@interface BrowserViewHiderCoordinator : ChromeCoordinator

// A reference to the view controller that implements the view revealing
// vertical pan handler delegate methods.
@property(nonatomic, weak, readonly) id<ViewRevealingAnimatee> animatee;

// The pan gesture handler for the hider view controller.
@property(nonatomic, weak) ViewRevealingVerticalPanHandler* panGestureHandler;

// The locationBarModel that informs this view about the current navigation
// entry.
@property(nonatomic, assign) LocationBarModel* locationBarModel;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_HIDER_BROWSER_VIEW_HIDER_COORDINATOR_H_
