// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/assistant/ui/assistant_container_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_view_state_delegate.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_consumer.h"
#import "ios/chrome/browser/intelligence/bwg/ui/gemini_container_mutator.h"
#import "ios/chrome/browser/intelligence/zero_state_suggestions/ui/gemini_zero_state_mutator.h"

namespace actor {
class ActorService;
}  // namespace actor

namespace gemini {
enum class EntryPoint;
}  // namespace gemini

class AuthenticationService;
class Browser;
class GeminiContainerMediatorEventHandler;
@class GeminiConfiguration;
@class GeminiGatewayManager;
@class GeminiPageContext;
@class GeminiStartupState;
@protocol AssistantContainerCommands;
@protocol BWGGatewayProtocol;
@protocol GeminiCommands;
@protocol GeminiSharedTabsDelegate;
@protocol GeminiZeroStateConsumer;

// Mediator for the Gemini container.
@interface GeminiContainerMediator : NSObject <AssistantContainerDelegate,
                                               GeminiContainerMutator,
                                               GeminiViewStateDelegate,
                                               GeminiZeroStateMutator>

// Delegate for shared tabs in a Gemini session.
@property(nonatomic, weak) id<GeminiSharedTabsDelegate> sharedTabsDelegate;

// Delegate for handling events from the mediator. Temporarily used by
// `GeminiBrowserAgent` to support pre-migration logic.
@property(nonatomic, assign) GeminiContainerMediatorEventHandler* eventHandler;

// Handler for container commands to update detent and grabber state.
@property(nonatomic, weak) id<AssistantContainerCommands> containerHandler;

// Handler for Gemini commands (e.g. dismissing Gemini flow).
@property(nonatomic, weak) id<GeminiCommands> geminiHandler;

// Consumer interface for handling UI updates from the coordinator.
@property(nonatomic, weak) id<GeminiContainerConsumer> consumer;

// The gateway for bridging internal protocols.
@property(nonatomic, readonly) id<BWGGatewayProtocol> gateway;

// Manager that creates and owns the gateway and handlers.
@property(nonatomic, readonly) GeminiGatewayManager* gatewayManager;

// Startup state used to initialize the Gemini content.
@property(nonatomic, strong) GeminiStartupState* startupState;

// Consumer for zero-state updates.
@property(nonatomic, weak) id<GeminiZeroStateConsumer> zeroStateConsumer;

// TODO(crbug.com/537719170): Mediator should be the target directly.
// Initializes the mediator with the given dependencies.
- (instancetype)initWithBrowser:(Browser*)browser
                   actorService:(actor::ActorService*)actorService
          authenticationService:(AuthenticationService*)authService
                   eventHandler:
                       (GeminiContainerMediatorEventHandler*)eventHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// TODO(crbug.com/535579970): Move to private after migration is complete.
// Creates and returns the GeminiConfiguration for the active web state.
- (GeminiConfiguration*)
    createGeminiConfigurationForActiveWebState:(GeminiStartupState*)startupState
                            baseViewController:
                                (UIViewController*)baseViewController;

// TODO(crbug.com/535579970): Move to private after migration is complete.
// Applies user preferences (e.g. page content sharing setting) to page context.
- (void)applyUserPrefsToPageContext:(GeminiPageContext*)geminiPageContext;

// Returns whether suggestion chips should be shown for the given entry point.
- (BOOL)shouldShowSuggestionChipsForEntryPoint:
    (gemini::EntryPoint)entryPoint;

// TODO(crbug.com/509898861): Move to private after migration is complete.
// Returns whether query submission should be blocked while page context is
// loading for the given entry point.
- (BOOL)shouldBlockQuerySubmissionWhileLoadingForEntryPoint:
    (gemini::EntryPoint)entryPoint;

// TODO(crbug.com/509898861): Move to private after migration is complete.
// Returns whether the page loading snackbar should be displayed on opening
// invocation for the given entry point.
- (BOOL)shouldShowPageLoadingSnackbarOnOpeningInvocationForEntryPoint:
    (gemini::EntryPoint)entryPoint;

// Handles initial setup for UI state and page context generation when the
// container session starts.
- (void)connect;

// Currently, `GeminiBrowserAgent` does some of the state setup/cleanup on each
// floaty invocation/dismissal, while full destruction happens on
// `GeminiBrowserAgent` destruction.
// `onFloatyInvoked` handles setup (such as attaching `WebState` observers) that
// should happen on floaty invocation in legacy mode.
// `onFloatyDismiss` handles all the cleanup that should happen on floaty
// dismissal.
// `disconnect` handles all the cleanup that should happen before
// mediator/`GeminiBrowserAgent` destruction.
// TODO(crbug.com/535579970): After the migration is done we can merge
// `onFloatyInvoked`/`onFloatyDismiss` into `connect`/`disconnect` as for the
// new code path the lifecycle of floaty and the mediator will be the same.
- (void)onFloatyInvoked;
- (void)onFloatyDismiss;

// Fetches zero-state suggestions for the active web state.
- (void)fetchZeroStateSuggestions:(GeminiStartupState*)startupState;

// Handles initial setup for UI state, page context generation, and connecting
// observed services (e.g. actor service) when the container session starts.
- (void)connect;

// Disconnects raw pointers owned by the mediator and dismisses handlers.
// Handles all the cleanup that needs to happen before mediator dealloc.
- (void)disconnect;

// Propagates active page context and shared tabs from `sharedTabsDelegate` to
// the provider.
- (void)propagatePageContext:(GeminiPageContext*)pageContext;

// Requests full page context generation for the active web state and propagates
// it to the provider upon completion.
- (void)requestActivePageContextGeneration;

// Updates the provider with partial page context for the active web state.
- (void)updateFloatyWithPartialPageContext;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_H_
