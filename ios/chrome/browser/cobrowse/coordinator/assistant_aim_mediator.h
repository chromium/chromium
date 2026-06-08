// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_
#define IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <vector>

#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_mutator.h"
#import "ios/chrome/browser/composebox/coordinator/composebox_url_loader.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"

@protocol AssistantContainerCommands;
@protocol SceneCommands;
@class CobrowseContext;
@class AimSRPDebuggerEvent;

namespace contextual_tasks {
class ContextualTasksService;
}
class UrlLoadingBrowserAgent;
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
@interface AssistantAIMMediator
    : NSObject <ComposeboxURLLoader, AssistantAIMMutator>

// The consumer for this mediator.
@property(nonatomic, weak) id<AssistantAIMConsumer> consumer;

// Handler for scene related commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// The delegate of the mediator.
@property(nonatomic, weak) id<AssistantAIMMediatorDelegate> delegate;

// Initializes the mediator with a web state and a cobrowse context that defines
// the AI mode assistant state, a container handler, the contextual tasks
// service, and the URL loader.
- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                         context:(CobrowseContext*)context
                containerHandler:
                    (id<AssistantContainerCommands>)containerHandler
          contextualTasksService:
              (contextual_tasks::ContextualTasksService*)contextualTasksService
                       URLLoader:(UrlLoadingBrowserAgent*)URLLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The logged events for AIM SRP communication.
@property(nonatomic, readonly) NSArray<AimSRPDebuggerEvent*>* debugEvents;
// Returns YES if the AIM page supports the given capability. Returns NO if
// the handshake has not completed yet or the capability is not supported.
- (BOOL)supportsCapability:(lens::FeatureCapability)capability;

// Returns the active capabilities of the current AIM page. Returns std::nullopt
// if the handshake has not completed yet.
- (const std::optional<std::vector<lens::FeatureCapability>>&)capabilities;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_
