// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_SEARCH_ENGINE_CHOICE_LEARN_MORE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_SEARCH_ENGINE_CHOICE_LEARN_MORE_COORDINATOR_H_

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

// If `YES`, the view controller is presented with UIModalPresentationFormSheet,
// otherwise the view controller is presented:
//  + For iPhone: UIModalPresentationPageSheet
//  + For iPad: UIModalPresentationFormSheet, using
//         kIPadSearchEngineChoiceScreenPreferredWidth
//         kIPadSearchEngineChoiceScreenPreferredHeight.
// The value has to be set before `start` is called.
@property(nonatomic, assign) BOOL forcePresentationFormSheet;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_LEARN_MORE_SEARCH_ENGINE_CHOICE_LEARN_MORE_COORDINATOR_H_
