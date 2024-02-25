// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_COORDINATOR_H_

#import "base/ios/block_types.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

class PromosManager;
@class SearchEngineChoiceCoordinator;
@protocol FirstRunScreenDelegate;

// The delegate for the choice screen coordinator
@protocol SearchEngineChoiceCoordinatorDelegate <NSObject>

// Called when the UI is dismissed.
- (void)choiceScreenWillBeDismissed:(SearchEngineChoiceCoordinator*)coordinator;

@end

// Coordinator for the search engine choice screen.
@interface SearchEngineChoiceCoordinator : ChromeCoordinator

// Delegate for the primary action.
@property(nonatomic, weak) id<SearchEngineChoiceCoordinatorDelegate> delegate;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

// Initiates a SearchEngineChoiceCoordinator with a `firstRun` parameter.
- (instancetype)initForFirstRunWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                                    browser:(Browser*)browser
                                           firstRunDelegate:
                                               (id<FirstRunScreenDelegate>)
                                                   delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_SEARCH_ENGINE_CHOICE_SEARCH_ENGINE_CHOICE_COORDINATOR_H_
