// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
#import "ios/chrome/browser/ui/gestures/view_revealing_animatee.h"

@class ViewRevealingVerticalPanHandler;
enum class ViewRevealState;

// Coordinator for the thumb strip, which is a 1-row horizontal display of tab
// miniatures above the toolbar.
@interface ThumbStripCoordinator : ChromeCoordinator <ViewRevealingAnimatee>

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              initialState:(ViewRevealState)initialState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The thumb strip's pan gesture handler.
@property(nonatomic, strong) ViewRevealingVerticalPanHandler* panHandler;

// The regular browser used to observe scroll events to show/hide the thumb
// strip.
@property(nonatomic, assign) Browser* regularBrowser;
// The incognito browser used to observe scroll events to show/hide the thumb
// strip.
@property(nonatomic, assign) Browser* incognitoBrowser;

@end

#endif  // IOS_CHROME_BROWSER_UI_THUMB_STRIP_THUMB_STRIP_COORDINATOR_H_
