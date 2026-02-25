// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CONFIGURATION_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <memory>

class AuthenticationService;
@class GeminiPageContext;
@protocol SingleSignOnService;

namespace gemini {
enum class EntryPoint;
}  // namespace gemini

namespace ios::provider {
enum class GeminiLocationPermissionState;
enum class BWGPageContextState;
enum class GeminiPageContextComputationState;
enum class GeminiPageContextAttachmentState;
}  // namespace ios::provider

namespace optimization_guide::proto {
class PageContext;
}  // namespace optimization_guide::proto

@protocol BWGGatewayProtocol;

// GeminiConfiguration is a configuration class that holds all the data
// necessary to start the Gemini overlay.
@interface GeminiConfiguration : NSObject

// The base view controller to present the UI on.
@property(nonatomic, weak) UIViewController* baseViewController;

// The page context and states necessary to include context in the floaty.
@property(nonatomic, strong) GeminiPageContext* pageContext;

// The PageContext for the current WebState. This is a unique_ptr, so subsequent
// calls to the getter will return a nullptr.
@property(nonatomic, assign)
    std::unique_ptr<optimization_guide::proto::PageContext>
        uniquePageContext;

// The state of the Gemini location permission.
@property(nonatomic, assign)
    ios::provider::GeminiLocationPermissionState geminiLocationPermissionState;

// The state of the Gemini PageContext computation.
@property(nonatomic, assign) ios::provider::GeminiPageContextComputationState
    geminiPageContextComputationState;

// The state of the BWG PageContext computation.
// TODO(crbug.com/467341090): Remove this property once all callers have
// migrated.
@property(nonatomic, assign) ios::provider::GeminiPageContextComputationState
    BWGPageContextComputationState;

// The state of the Gemini PageContext attachment.
@property(nonatomic, assign) ios::provider::GeminiPageContextAttachmentState
    geminiPageContextAttachmentState;

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

// Image to be attached to the Gemini instance.
@property(nonatomic, strong) UIImage* imageAttachment;

// Whether to show the Gemini image remix in-product help in the Floaty.
@property(nonatomic, assign) BOOL imageRemixIPHShouldShow;

// Whether the backend migration is enabled.
@property(nonatomic, assign) BOOL backendMigrationEnabled;

// Whether Gemini actor is enabled.
@property(nonatomic, assign) BOOL geminiActorEnabled;

// Whether to use the response ready interval to show the response ready
// notification in the floaty.
@property(nonatomic, assign) double responseReadyInterval;

// Whether to use the dynamic size for the response view in the floaty.
@property(nonatomic, assign) BOOL responseViewDynamicSizeEnabled;

// Whether to show the zero state with chat history in the floaty.
@property(nonatomic, assign)
    BOOL geminiCopresenceZeroStateWithChatHistoryEnabled;

// The initial bottom offset of the floaty.
@property(nonatomic, assign) CGFloat initialBottomOffset;

// The window scene in which the Gemini view window is initialized and
// presented.
@property(nonatomic, strong) UIWindowScene* hostWindowScene;

// The entry point where the floaty was triggered from.
@property(nonatomic, assign) gemini::EntryPoint entryPoint;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_CONFIGURATION_H_
