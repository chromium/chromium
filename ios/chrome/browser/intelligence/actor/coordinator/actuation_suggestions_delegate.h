// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_SUGGESTIONS_DELEGATE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_SUGGESTIONS_DELEGATE_H_

#import <Foundation/Foundation.h>

@class ActorFormSuggestion;
@class ActuationSuggestionsCoordinator;

// Delegate protocol for handling suggestion selection events.
@protocol ActuationSuggestionsDelegate <NSObject>

// Notifies the delegate that `suggestion` was selected in `coordinator`.
- (void)suggestionsCoordinator:(ActuationSuggestionsCoordinator*)coordinator
           didSelectSuggestion:(ActorFormSuggestion*)suggestion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_SUGGESTIONS_DELEGATE_H_
