// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_STARTUP_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_STARTUP_CONFIGURATION_H_

#import <Foundation/Foundation.h>

class AuthenticationService;

@protocol BWGGatewayProtocol;

// `GeminiStartupConfiguration` is a configuration class that holds all the data
// necessary to configure Gemini at startup.
@interface GeminiStartupConfiguration : NSObject

// The authentication service to be used.
@property(nonatomic, assign) AuthenticationService* authService;

// The BWG gateway for bridging internal protocols.
@property(nonatomic, weak) id<BWGGatewayProtocol> gateway;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_STARTUP_CONFIGURATION_H_
