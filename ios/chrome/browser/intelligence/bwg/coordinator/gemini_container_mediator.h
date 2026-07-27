// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_H_

#import <UIKit/UIKit.h>

namespace gemini {
enum class EntryPoint;
}  // namespace gemini

class ProfileIOS;
class WebStateList;
@class GeminiConfiguration;
@class GeminiStartupState;
@protocol BWGGatewayProtocol;

// Mediator for the Gemini container.
@interface GeminiContainerMediator : NSObject

// Initializes the mediator with the given dependencies.
// TODO(crbug.com/532334416): Instead of passing the gateway, it should be
// created and owned by the mediator.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                             profile:(ProfileIOS*)profile
                             gateway:(id<BWGGatewayProtocol>)gateway
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// TODO(crbug.com/535579970): Move to private after migration is complete.
// Creates and returns the GeminiConfiguration for the active web state.
- (GeminiConfiguration*)createGeminiConfigurationForActiveWebState:
    (GeminiStartupState*)startupState;

// Returns whether suggestion chips should be shown for the given entry point.
- (BOOL)shouldShowSuggestionChipsForEntryPoint:
    (gemini::EntryPoint)entryPoint;

// Disconnects the mediator and performs necessary cleanup.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_COORDINATOR_GEMINI_CONTAINER_MEDIATOR_H_
