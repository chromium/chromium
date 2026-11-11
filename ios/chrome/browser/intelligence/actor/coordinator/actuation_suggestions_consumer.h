// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_SUGGESTIONS_CONSUMER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_SUGGESTIONS_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ActorFormSuggestion;
@class ActuationWorklogItem;

// Consumer protocol for receiving suggestions to be coordinated and presented.
@protocol ActuationSuggestionsConsumer <NSObject>

// Presents `suggestions` for the active `worklogItem`, or dismisses any
// presented suggestions if `suggestions` is nil. Setting the presented
// `suggestions` again only updates their `worklogItem`, e.g. after a
// suggestion reselection.
- (void)setSuggestions:(NSArray<ActorFormSuggestion*>*)suggestions
    forActiveWorklogItem:(ActuationWorklogItem*)worklogItem;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_SUGGESTIONS_CONSUMER_H_
