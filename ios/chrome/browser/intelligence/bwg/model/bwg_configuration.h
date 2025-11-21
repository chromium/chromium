// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_CONFIGURATION_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <memory>

class AuthenticationService;
@class GeminiPageContext;
@protocol SingleSignOnService;

namespace ios::provider {
enum class BWGLocationPermissionState;
enum class BWGPageContextState;
enum class BWGPageContextComputationState;
enum class BWGPageContextAttachmentState;
}  // namespace ios::provider

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

@protocol BWGGatewayProtocol;

// BWGConfiguration is a configuration class that holds all the data necessary
// to start the BWG overlay.
@interface BWGConfiguration : NSObject

// The base view controller to present the UI on.
@property(nonatomic, weak) UIViewController* baseViewController;

// The page context and states necessary to include context in the floaty.
@property(nonatomic, strong) GeminiPageContext* pageContext;

// The PageContext for the current WebState. This is a unique_ptr, so subsequent
// calls to the getter will return a nullptr.
@property(nonatomic, assign)
    std::unique_ptr<optimization_guide::proto::PageContext>
        uniquePageContext;

// The state of the BWG location permission.
@property(nonatomic, assign)
    ios::provider::BWGLocationPermissionState BWGLocationPermissionState;

// The state of the BWG PageContext computation.
@property(nonatomic, assign) ios::provider::BWGPageContextComputationState
    BWGPageContextComputationState;

// The state of the BWG PageContext attachment.
@property(nonatomic, assign)
    ios::provider::BWGPageContextAttachmentState BWGPageContextAttachmentState;

// The favicon of the attached page. Uses a default icon if it's unavailable.
@property(nonatomic, strong) UIImage* favicon;

// The authentication service to be used.
@property(nonatomic, assign) AuthenticationService* authService;

// The SingleSignOnService instance.
@property(nonatomic, strong) id<SingleSignOnService> singleSignOnService;

// The BWG gateway for bridging internal protocols.
@property(nonatomic, weak) id<BWGGatewayProtocol> gateway;

// The client ID, uniquely representing the WebState.
@property(nonatomic, copy) NSString* clientID;

// The server ID, uniquely representing the session at the server level.
@property(nonatomic, copy) NSString* serverID;

// Whether to animate the presentation of the BWG UI.
@property(nonatomic, assign) BOOL shouldAnimatePresentation;

// Whether the last interaction was completed on a different URL (ignoring
// fragments).
@property(nonatomic, assign) BOOL lastInteractionURLDifferent;

// Whether the zero-state suggestion chips should be shown.
@property(nonatomic, assign) BOOL shouldShowSuggestionChips;

// Label displayed from a Gemini contextual cue chip.
@property(nonatomic, copy) NSString* contextualCueChipLabel;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_CONFIGURATION_H_
