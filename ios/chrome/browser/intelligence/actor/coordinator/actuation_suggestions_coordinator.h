// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_SUGGESTIONS_COORDINATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_SUGGESTIONS_COORDINATOR_H_

#import "ios/chrome/browser/intelligence/actor/ui/actuation_suggestions_presenter.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@class ActorFormSuggestion;
@class ActuationWorklogItem;

// Coordinator managing suggestion picker presentation and selection.
@interface ActuationSuggestionsCoordinator
    : ChromeCoordinator <ActuationSuggestionsPresenter>

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               suggestions:
                                   (NSArray<ActorFormSuggestion*>*)suggestions
                               worklogItem:(ActuationWorklogItem*)worklogItem
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_SUGGESTIONS_COORDINATOR_H_
