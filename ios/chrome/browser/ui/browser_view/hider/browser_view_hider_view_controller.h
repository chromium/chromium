// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_HIDER_BROWSER_VIEW_HIDER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_HIDER_BROWSER_VIEW_HIDER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_steady_view_consumer.h"

@class ViewRevealingVerticalPanHandler;

// View controller for a view that hides the browser view controller's view.
@interface BrowserViewHiderViewController
    : UIViewController <LocationBarSteadyViewConsumer, ViewRevealingAnimatee>

// Pan gesture handler for the hider view.
@property(nonatomic, weak) ViewRevealingVerticalPanHandler* panGestureHandler;

@property(nonatomic, assign) BOOL incognito;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_HIDER_BROWSER_VIEW_HIDER_VIEW_CONTROLLER_H_
