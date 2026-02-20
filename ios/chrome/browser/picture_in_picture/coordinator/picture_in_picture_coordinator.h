// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_COORDINATOR_PICTURE_IN_PICTURE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_COORDINATOR_PICTURE_IN_PICTURE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class PictureInPictureConfiguration;

// Coordinator to present a video in picture-in-picture.
@interface PictureInPictureCoordinator : ChromeCoordinator

// Initializes the coordinator with the given configuration.
- (instancetype)initWithConfiguration:
                    (PictureInPictureConfiguration*)configuration
                   baseViewController:(UIViewController*)viewController
                              browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

// Unavailable initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Dismisses picture in picture if the user returned to the app manually instead
// of using the picture in picture restore action.
- (void)dismissIfNotPipRestore;

@end

#endif  // IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_COORDINATOR_PICTURE_IN_PICTURE_COORDINATOR_H_
