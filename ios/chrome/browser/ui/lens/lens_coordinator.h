// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LENS_LENS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_LENS_LENS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class LensCoordinator;

// A protocol for view controllers that wish to present a Lens experience.
@protocol LensPresentationDelegate

// Returns the web content frame for the Lens Coordinator to use for
// animations.
- (CGRect)webContentAreaForLensCoordinator:(LensCoordinator*)lensCoordinator;

@end

// LensCoordinator presents the public interface for Lens related features.
@interface LensCoordinator : ChromeCoordinator

// Initializes this Coordinator with its `browser` and a nil base view
// controller.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// The base view controller for this coordinator.
@property(weak, nonatomic, readwrite) UIViewController* baseViewController;

// The presentation delegate for this coordinator.
@property(weak, nonatomic, readwrite) id<LensPresentationDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_LENS_LENS_COORDINATOR_H_
