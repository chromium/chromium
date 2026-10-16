// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_COORDINATOR_SEARCH_ENGINE_CHOICE_LEARN_MORE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_COORDINATOR_SEARCH_ENGINE_CHOICE_LEARN_MORE_COORDINATOR_H_

#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

// The delegate for the "Learn More" coordinator
@protocol SearchEngineChoiceLearnMoreCoordinatorDelegate <NSObject>

// Called when the popup is dismissed.
- (void)learnMoreDidDismiss;

@end

// Coordinator for the "Why am I seeing this" informational popup triggered
// when the user taps the "Why Am I seeing this" button.
@interface SearchEngineChoiceLearnMoreCoordinator : ChromeCoordinator

// Delegate for all the user actions.
@property(nonatomic, weak) id<SearchEngineChoiceLearnMoreCoordinatorDelegate>
    delegate;

// Whether the learn more screen is being presented during the First Run
// Experience (FRE). When `NO`, `preferredContentSize` is set to match the
// standalone Search Engine Choice screen's preferred dimensions.
// The value has to be set before `start` is called.
@property(nonatomic, assign) BOOL firstRun;

@end

#endif  // IOS_CHROME_BROWSER_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_COORDINATOR_SEARCH_ENGINE_CHOICE_LEARN_MORE_COORDINATOR_H_
