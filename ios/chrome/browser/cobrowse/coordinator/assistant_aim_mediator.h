// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_url_loader.h"

@protocol AssistantContainerCommands;
@class CobrowseContext;

namespace contextual_tasks {
class ContextualTasksService;
}

namespace web {
class WebState;
}

@class AssistantAIMMediator;

// Delegate for the Assistant AIM Mediator.
@protocol AssistantAIMMediatorDelegate <NSObject>

// Called after a query is loaded.
- (void)assistantAIMMediatorDidLoadQuery:(AssistantAIMMediator*)mediator;

@end

// Mediator that manages the business logic and data for the AI mode Assistant.
@interface AssistantAIMMediator : NSObject <ComposeboxURLLoader>

// The consumer for this mediator.
@property(nonatomic, weak) id<AssistantAIMConsumer> consumer;

// Initializes the mediator with a web state and a cobrowse context that defines
// the AI mode assistant state, a container handler, and the contextual tasks
// service.
- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                         context:(CobrowseContext*)context
                containerHandler:
                    (id<AssistantContainerCommands>)containerHandler
          contextualTasksService:
              (contextual_tasks::ContextualTasksService*)contextualTasksService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The delegate of the mediator.
@property(nonatomic, weak) id<AssistantAIMMediatorDelegate> delegate;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_
