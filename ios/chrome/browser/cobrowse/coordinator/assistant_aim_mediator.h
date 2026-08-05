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
class GURL;
@class CobrowseContext;
@class AimSRPDebuggerEvent;

namespace contextual_tasks {
class ContextualTasksService;
}
class UrlLoadingBrowserAgent;
class CobrowseBrowserAgent;
class AuthenticationService;
namespace web {
class WebState;
}

@class AssistantAIMMediator;

// Delegate for the Assistant AIM Mediator.
@protocol AssistantAIMMediatorDelegate <NSObject>

// Called after a query is loaded.
- (void)assistantAIMMediatorDidLoadQuery:(AssistantAIMMediator*)mediator;

// Called when the mediator starts a new thread.
- (void)assistantAIMMediatorDidStartNewThread:(AssistantAIMMediator*)mediator;

// Called when the mediator reacts to a tap in the minimized state.
- (void)assistantAIMMediatorDidFocusFromMinimized:
    (AssistantAIMMediator*)mediator;

// Called when the server's Thread Context Library updates with a webpage
// context attachment. "WebpageSignal" carries the URL and title of a webpage
// context associated with the active thread. The delegate forwards this signal
// to the Composebox Input Plate to populate and display the corresponding
// webpage context chip in the UI.
- (void)assistantAIMMediator:(AssistantAIMMediator*)mediator
    didReceiveContextLibraryWebpageSignalWithURL:(const GURL&)url
                                           title:(NSString*)title;

@end

// Mediator that manages the business logic and data for the AI mode Assistant.
@interface AssistantAIMMediator
    : NSObject <AssistantAIMMutator, ComposeboxURLLoader>

// The consumer for this mediator.
@property(nonatomic, weak) id<AssistantAIMConsumer> consumer;

// Handler for scene related commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// The delegate of the mediator.
@property(nonatomic, weak) id<AssistantAIMMediatorDelegate> delegate;

// Initializes the mediator with a web state and a cobrowse browser agent that
// defines the AI mode assistant state, a container handler, the contextual
// tasks service, the URL loader, and the authentication service.
- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
            cobrowseBrowserAgent:(CobrowseBrowserAgent*)cobrowseBrowserAgent
                containerHandler:
                    (id<AssistantContainerCommands>)containerHandler
          contextualTasksService:
              (contextual_tasks::ContextualTasksService*)contextualTasksService
                       URLLoader:(UrlLoadingBrowserAgent*)URLLoader
           authenticationService:(AuthenticationService*)authenticationService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The logged events for AIM SRP communication.
@property(nonatomic, readonly) NSArray<AimSRPDebuggerEvent*>* debugEvents;

// The currently loaded AIM URL.
@property(nonatomic, readonly) GURL loadedURL;

// Instructs the mediator to navigate the web state to the given URL.
// For debugging and testing only. Has no effect unless omnibox debugging
// is enabled.
- (void)loadURL:(const GURL&)url;

// Returns YES if the AIM page supports the given capability. Returns NO if
// the handshake has not completed yet or the capability is not supported.
- (BOOL)supportsCapability:(lens::FeatureCapability)capability;

// Returns the active capabilities of the current AIM page. Returns std::nullopt
// if the handshake has not completed yet.
- (const std::optional<std::vector<lens::FeatureCapability>>&)capabilities;

// Updates the context from the browser agent and reloads if it has changed.
- (void)updateContext;

// Disconnects the mediator.
- (void)disconnect;

// Ends the current cobrowse session.
- (void)endSession;

// Called when the user interface style (light/dark mode) changes.
- (void)updateDarkModeState:(BOOL)isDarkMode;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_COORDINATOR_ASSISTANT_AIM_MEDIATOR_H_
