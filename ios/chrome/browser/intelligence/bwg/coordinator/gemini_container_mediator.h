// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_H_

#import <UIKit/UIKit.h>

namespace gemini {
enum class EntryPoint;
}  // namespace gemini

class Browser;
class WebStateList;
class GeminiViewStateChangeHandlerTarget;
@class GeminiConfiguration;
@class GeminiGatewayManager;
@class GeminiPageContext;
@class GeminiStartupState;
@protocol BWGGatewayProtocol;

// Mediator for the Gemini container.
@interface GeminiContainerMediator : NSObject

// The gateway for bridging internal protocols.
@property(nonatomic, readonly) id<BWGGatewayProtocol> gateway;

// Manager that creates and owns the gateway and handlers.
@property(nonatomic, readonly) GeminiGatewayManager* gatewayManager;

// TODO(crbug.com/537719170): Mediator should be the target directly.
// Initializes the mediator with the given dependencies.
- (instancetype)initWithBrowser:(Browser*)browser
                         target:(GeminiViewStateChangeHandlerTarget*)target
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// TODO(crbug.com/535579970): Move to private after migration is complete.
// Creates and returns the GeminiConfiguration for the active web state.
- (GeminiConfiguration*)
    createGeminiConfigurationForActiveWebState:(GeminiStartupState*)startupState
                            baseViewController:
                                (UIViewController*)baseViewController;

// TODO(crbug.com/535579970): Move to private after migration is complete.
// Configures Gemini with startup parameters.
- (void)configureGemini;

// TODO(crbug.com/535579970): Move to private after migration is complete.
// Applies user preferences (e.g. page content sharing setting) to page context.
- (void)applyUserPrefsToPageContext:(GeminiPageContext*)geminiPageContext;

// Returns whether suggestion chips should be shown for the given entry point.
- (BOOL)shouldShowSuggestionChipsForEntryPoint:
    (gemini::EntryPoint)entryPoint;

// Currently, `GeminiBrowserAgent` does some of the state cleanup after each
// floaty dismissal, but some of the cleanup such as releasing the handlers
// happens on GeminiBrowserAgent destruction.
// `onFloatyDismiss` handles all the cleanup that should happen on floaty
// dismissal.
// `disconnect` handles all the cleanup that should happen before
// mediator/`GeminiBrowserAgent` destruction.
// TODO(crbug.com/535579970): After the migration is done we can merge
// `onFloatyDismiss` and `disconnect` as for the new code path the lifcylce
// of floaty and the mediator will be the same, meaning the mediator will be
// destructed on each floaty dismissal.
- (void)onFloatyDismiss;

// Disconnects raw pointers owned by the mediator and dismisses handlers.
// Handles all the cleanup that needs to happen before mediator dealloc.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_H_
