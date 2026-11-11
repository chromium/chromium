// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/intelligence/actor/coordinator/actuation_suggestions_delegate.h"
#import "ios/chrome/browser/intelligence/actor/ui/actuation_worklog_mutator.h"

@protocol ActorTaskActionHandler;
@protocol ActorTaskDataSource;
@protocol ActuationSuggestionsConsumer;
@protocol ActuationWorklogConsumer;

// Displays the worklog states published by an `ActorTaskDataSource` in an
// `ActuationWorklogConsumer`, sending only the entries that changed.
@interface ActuationWorklogMediator
    : NSObject <ActuationSuggestionsDelegate, ActuationWorklogMutator>

// The consumer that receives formatted worklog updates.
@property(nonatomic, weak) id<ActuationWorklogConsumer> consumer;

// Consumer handling suggestion coordinator lifecycle.
@property(nonatomic, weak) id<ActuationSuggestionsConsumer> suggestionsConsumer;

// Starts displaying the worklog published by `dataSource`, replacing any
// previously observed one. The user actions on the worklog are forwarded to
// `actionHandler`.
- (void)startObservingDataSource:(id<ActorTaskDataSource>)dataSource
                   actionHandler:(id<ActorTaskActionHandler>)actionHandler;

// Stops observing the current data source and resets the worklog. No-op if
// none is observed.
- (void)stopObservingDataSource;

// Disconnects the mediator and cleans up references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_COORDINATOR_ACTUATION_WORKLOG_MEDIATOR_H_
