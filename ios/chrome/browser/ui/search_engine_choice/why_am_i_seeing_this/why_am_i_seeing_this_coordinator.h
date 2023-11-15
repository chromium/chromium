// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_WHY_AM_I_SEEING_THIS_WHY_AM_I_SEEING_THIS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_WHY_AM_I_SEEING_THIS_WHY_AM_I_SEEING_THIS_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// The delegate for the "Learn More" coordinator
@protocol LearnMoreCoordinatorDelegate <NSObject>

// Called when the popup is dismissed.
- (void)learnMoreDidDismiss;

@end

// Coordinator for the "Why am I seeing this" informational popup triggered
// when the user taps the "Why Am I seeing this" button.
// TODO(b/306123679) Rename this class to LearnMoreCoordinator
@interface WhyAmISeeingThisCoordinator : ChromeCoordinator

// Delegate for all the user actions.
@property(nonatomic, weak) id<LearnMoreCoordinatorDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_WHY_AM_I_SEEING_THIS_WHY_AM_I_SEEING_THIS_COORDINATOR_H_
