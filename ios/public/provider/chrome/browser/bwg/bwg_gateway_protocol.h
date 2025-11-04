// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_GATEWAY_PROTOCOL_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_GATEWAY_PROTOCOL_H_

#import <Foundation/Foundation.h>

@protocol BWGLinkOpeningDelegate;
@protocol BWGPageStateChangeDelegate;
@protocol BWGSessionDelegate;
@protocol GeminiSuggestionDelegate;

// Protocol for the BWG gateway, exposing what's needed upstream.
@protocol BWGGatewayProtocol

// Handlers for BWG protocols.
@property(nonatomic, weak) id<BWGLinkOpeningDelegate> linkOpeningHandler;
@property(nonatomic, weak) id<BWGPageStateChangeDelegate>
    pageStateChangeHandler;
@property(nonatomic, weak) id<BWGSessionDelegate> sessionHandler;
@property(nonatomic, weak) id<GeminiSuggestionDelegate> suggestionHandler;

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BWG_BWG_GATEWAY_PROTOCOL_H_
