// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_GEMINI_SUGGESTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_GEMINI_SUGGESTIONS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class GeminiSuggestionsCoordinator;

// Delegate for `GeminiSuggestionsCoordinator`.
@protocol GeminiSuggestionsCoordinatorDelegate <NSObject>

// Called when the view controller is removed from navigation controller.
- (void)geminiSuggestionsCoordinatorViewControllerWasRemoved:
    (GeminiSuggestionsCoordinator*)coordinator;

@end

// Coordinator for the Gemini suggestions settings view.
@interface GeminiSuggestionsCoordinator : ChromeCoordinator

// Delegate for coordinator lifecycle events.
@property(nonatomic, weak) id<GeminiSuggestionsCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Designated initializer.
// `navigationController`: navigation controller.
// `browser`: browser.
- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_BWG_COORDINATOR_GEMINI_SUGGESTIONS_COORDINATOR_H_
