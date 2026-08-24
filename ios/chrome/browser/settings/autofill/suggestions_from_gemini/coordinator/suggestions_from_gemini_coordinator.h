// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_COORDINATOR_SUGGESTIONS_FROM_GEMINI_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_COORDINATOR_SUGGESTIONS_FROM_GEMINI_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class SuggestionsFromGeminiCoordinator;

// Delegate for SuggestionsFromGeminiCoordinator.
@protocol SuggestionsFromGeminiCoordinatorDelegate <NSObject>

// Called when the coordinator is finished and should be removed.
- (void)suggestionsFromGeminiCoordinatorDidRemove:
    (SuggestionsFromGeminiCoordinator*)coordinator;

@end

// Coordinator for the Suggestions from Gemini settings.
@interface SuggestionsFromGeminiCoordinator : ChromeCoordinator

// Delegate.
@property(nonatomic, weak) id<SuggestionsFromGeminiCoordinatorDelegate>
    delegate;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_SUGGESTIONS_FROM_GEMINI_COORDINATOR_SUGGESTIONS_FROM_GEMINI_COORDINATOR_H_
