// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"
@class GeminiStartupState;

// Coordinator for managing the presentation and lifecycle of the Gemini
// Container.
@interface GeminiContainerCoordinator : ChromeCoordinator

// Initializes the coordinator with the given startup state, base view
// controller, and browser.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                              startupState:(GeminiStartupState*)startupState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Dismisses the assistant container.
- (void)dismissWithCompletion:(void (^)(void))completion;

// Minimizes the assistant container to its smallest detent while keeping it
// visible.
- (void)minimize;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_COORDINATOR_H_
