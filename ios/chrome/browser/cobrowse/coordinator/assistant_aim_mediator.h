// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_mutator.h"

@protocol AssistantAIMConsumer;

namespace web {
class WebState;
}

// Mediator for the Assistant AIM UI.
@interface AssistantAIMMediator : NSObject <AssistantAIMMutator>

// The consumer for this mediator.
@property(nonatomic, weak) id<AssistantAIMConsumer> consumer;

// Initializes the mediator with a WebState.
- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_
