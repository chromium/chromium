// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_GATEWAY_MANAGER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_GATEWAY_MANAGER_H_

#import <Foundation/Foundation.h>

class Browser;
@class GeminiActuationHandler;
@class GeminiCameraHandler;
@class GeminiConsentProviderHandler;
@class GeminiLinkOpeningHandler;
@class GeminiPageStateChangeHandler;
@class GeminiSessionHandler;
@class GeminiSuggestionHandler;
@class GeminiTabPickerHandler;
@protocol BWGGatewayProtocol;
@protocol GeminiViewStateDelegate;

// Manager class for creating, initializing, and holding ownership of Gemini
// gateway objects and their handlers.
@interface GeminiGatewayManager : NSObject

// The gateway for bridging internal protocols.
@property(nonatomic, readonly) id<BWGGatewayProtocol> gateway;

// TODO(crbug.com/491093929): Rename the below classes to move away from the
// `-Handler` naming scheme used by Chromium Objective-C command protocols.
// Handlers created and owned by the manager.
@property(nonatomic, readonly) GeminiLinkOpeningHandler* linkOpeningHandler;
@property(nonatomic, readonly)
    GeminiPageStateChangeHandler* pageStateChangeHandler;
@property(nonatomic, readonly) GeminiSessionHandler* sessionHandler;
@property(nonatomic, readonly) GeminiCameraHandler* cameraHandler;
@property(nonatomic, readonly) GeminiTabPickerHandler* tabPickerHandler;
@property(nonatomic, readonly)
    GeminiConsentProviderHandler* consentProviderHandler;
@property(nonatomic, readonly) GeminiSuggestionHandler* suggestionHandler;
@property(nonatomic, readonly) GeminiActuationHandler* actuationHandler;

// Initializes the manager and sets up gateway and handlers for the given
// browser and view state delegate.
- (instancetype)initWithBrowser:(Browser*)browser
              viewStateDelegate:(id<GeminiViewStateDelegate>)viewStateDelegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects handlers owned by the manager.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_GATEWAY_MANAGER_H_
