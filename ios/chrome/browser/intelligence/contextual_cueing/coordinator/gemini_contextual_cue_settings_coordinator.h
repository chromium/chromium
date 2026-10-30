// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_COORDINATOR_GEMINI_CONTEXTUAL_CUE_SETTINGS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_COORDINATOR_GEMINI_CONTEXTUAL_CUE_SETTINGS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class GeminiContextualCueSettingsCoordinator;

// Delegate for `GeminiContextualCueSettingsCoordinator`.
@protocol GeminiContextualCueSettingsCoordinatorDelegate <NSObject>

// Called when the contextual cue settings modal is dismissed.
- (void)geminiContextualCueSettingsCoordinatorDidDismiss:
    (GeminiContextualCueSettingsCoordinator*)coordinator;

@end

// Coordinator for the Gemini contextual cue settings bottom sheet modal.
@interface GeminiContextualCueSettingsCoordinator : ChromeCoordinator

#pragma mark - Public

// Delegate for coordinator lifecycle events.
@property(nonatomic, weak) id<GeminiContextualCueSettingsCoordinatorDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_CONTEXTUAL_CUEING_COORDINATOR_GEMINI_CONTEXTUAL_CUE_SETTINGS_COORDINATOR_H_
