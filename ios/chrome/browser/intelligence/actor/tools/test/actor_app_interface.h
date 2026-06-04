// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_TEST_ACTOR_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_TEST_ACTOR_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// Error domain for ActorAppInterface.
extern NSString* const kActorAppInterfaceErrorDomain;

// Error codes for ActorAppInterface.
typedef NS_ENUM(NSInteger, ActorAppInterfaceErrorCode) {
  ActorToolExecutionResultNoProfile = 1,
  ActorToolExecutionResultNoService = 2,
  ActorToolExecutionResultInvalidProto = 3,
  ActorToolExecutionResultNoActuationResults = 4,
  ActorToolExecutionResultNoWebState = 5,
  ActorToolExecutionResultNoMainFrame = 6,
};

// App interface to interact with the ActorService from integration tests.
@interface ActorAppInterface : NSObject

// Executes an Action defined by the serialized proto.
// The completion block is called when the action finishes or fails.
+ (void)executeActionWithProto:(NSData*)actionProto
                    completion:(void (^)(NSError* error))completion;

// Fetches the latest Annotated Page Content (APC) via the PageContextWrapper
// and returns the serialized optimization_guide::proto::PageContext.
+ (NSData*)fetchLatestAPC;

// Waits for page stability in the current main frame.
+ (void)waitForPageStabilityWithCompletion:(void (^)(NSError* error))completion;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_TOOLS_TEST_ACTOR_APP_INTERFACE_H_
