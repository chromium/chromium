// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_DATA_TYPES_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_DATA_TYPES_H_

#import <Foundation/Foundation.h>

#import <string>

#import "components/actor/public/mojom/actor_types.mojom.h"

// Reasons why Gemini yields control back to the user.
enum class GeminiYieldReason {
  // Unknown or unrecognized reason.
  kUnknownReason = 0,
  // Waiting for user confirmation.
  kConfirmation = 1,
  // Waiting for user clarification.
  kClarification = 2,
  // Waiting for user takeover.
  kUserTakeover = 3,
  // Task is complete.
  kTaskComplete = 4,
  // User provided irrelevant input.
  kIrrelevantUserInput = 5,
};

// Encapsulates a yield instruction, directing Chrome to yield control to the
// user.
@interface GeminiYieldAction : NSObject

// The reason why Gemini is yielding to the user.
@property(nonatomic, readonly, assign) GeminiYieldReason reason;

// Optional prompt or explanatory message to display to the user.
@property(nonatomic, readonly, copy) NSString* messageToUser;

// Initializes with the yield reason and optional message to display to the
// user.
- (instancetype)initWithReason:(GeminiYieldReason)reason
                 messageToUser:(NSString*)messageToUser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// Represents an incoming actuation request dispatched from Gemini, containing
// either actions to execute or a yield instruction.
@interface GeminiActuationRequest : NSObject

// Serialized action protobufs to execute on the task's controlled WebStates,
// or nil if this request is a yield instruction.
@property(nonatomic, readonly, copy) NSArray<NSData*>* actionProtos;

// Optional progress status message for the action execution.
@property(nonatomic, readonly, copy) NSString* taskUpdate;

// Yield instruction, or nil if this request is for executing actions.
@property(nonatomic, readonly, strong) GeminiYieldAction* yieldAction;

// Initializes an actuation request with action protos, task update, and yield
// action. `actionProtos` and `yieldAction` are mutually exclusive; this is
// enforced in `GeminiActuationHandler
// dispatchActuationRequest:forTaskID:completionBlock:`.
- (instancetype)initWithActionProtos:(NSArray<NSData*>*)actionProtos
                          taskUpdate:(NSString*)taskUpdate
                         yieldAction:(GeminiYieldAction*)yieldAction
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

// Encapsulates the response to a `GeminiActuationRequest`.
@interface GeminiActuationResponse : NSObject

// Serialized `ActionsResult` protobuf returned by Chrome's Actor Service.
@property(nonatomic, readonly, copy) NSData* serializedActionsResult;

// Execution status code.
@property(nonatomic, readonly, assign)
    actor::mojom::ActionResultCode resultCode;

// Optional user response text (e.g. confirmation or clarification).
@property(nonatomic, readonly, copy) NSString* userResponse;

// Initializes an actuation response with result code, optional user response,
// and optional serialized actions result.
- (instancetype)initWithResultCode:(actor::mojom::ActionResultCode)resultCode
                      userResponse:(NSString*)userResponse
           serializedActionsResult:(NSData*)serializedActionsResult
    NS_DESIGNATED_INITIALIZER;

// Initializes an actuation response representing a failure with the given
// `resultCode` and `errorMessage`.
- (instancetype)initWithResultCode:(actor::mojom::ActionResultCode)resultCode
                      errorMessage:(const std::string&)errorMessage;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_GEMINI_ACTUATION_DATA_TYPES_H_
