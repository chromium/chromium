// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_DATA_TYPES_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_DATA_TYPES_H_

#import <Foundation/Foundation.h>

#import "components/actor/public/mojom/actor_types.mojom.h"

// Represents an incoming TTC actuation request,
// containing serialized action protobufs to execute on controlled WebStates.
@interface TTCActuationRequest : NSObject

// Serialized `optimization_guide::proto::Action` protobufs to execute.
@property(nonatomic, readonly, copy) NSArray<NSData*>* actionProtos;

// Optional progress status message describing the action execution.
@property(nonatomic, readonly, copy) NSString* taskUpdate;

// Correlation identifiers associated with the client tool calls.
@property(nonatomic, readonly, copy) NSArray<NSString*>* callIDs;

// Convenience accessor returning the primary (first) correlation identifier.
@property(nonatomic, readonly, copy) NSString* callID;

// Initializes an actuation request with serialized action protobufs, an
// optional task update, and an array of correlation call IDs.
// @param actionProtos Serialized `Action` protobufs to execute.
// @param taskUpdate Optional human-readable progress status.
// @param callIDs Correlation identifiers matching client tool calls.
- (instancetype)initWithActionProtos:(NSArray<NSData*>*)actionProtos
                          taskUpdate:(NSString*)taskUpdate
                             callIDs:(NSArray<NSString*>*)callIDs
    NS_DESIGNATED_INITIALIZER;

// Convenience initializer for a single tool call request.
// @param actionProtos Serialized `Action` protobufs to execute.
// @param taskUpdate Optional human-readable progress status.
// @param callID Primary correlation call ID.
- (instancetype)initWithActionProtos:(NSArray<NSData*>*)actionProtos
                          taskUpdate:(NSString*)taskUpdate
                              callID:(NSString*)callID;

- (instancetype)init NS_UNAVAILABLE;

@end

// Encapsulates the execution response for a `TTCActuationRequest`.
@interface TTCActuationResponse : NSObject

// Execution status code returned by Chrome's Actor Service.
@property(nonatomic, readonly, assign)
    actor::mojom::ActionResultCode resultCode;

// Descriptive error message if the actuation failed, or nil on success.
@property(nonatomic, readonly, copy) NSString* errorMessage;

// Serialized `optimization_guide::proto::ActionsResult` protobuf, or nil.
@property(nonatomic, readonly, copy) NSData* serializedActionsResult;

// Correlation identifier matching the request's `callID`.
@property(nonatomic, readonly, copy) NSString* callID;

// Initializes an actuation response with result code, error message,
// serialized actions result proto, and correlation call ID.
// @param resultCode Status code returned by ActorService.
// @param errorMessage Optional failure description.
// @param serializedActionsResult Optional serialized `ActionsResult` protobuf.
// @param callID Correlation call ID.
- (instancetype)initWithResultCode:(actor::mojom::ActionResultCode)resultCode
                      errorMessage:(NSString*)errorMessage
           serializedActionsResult:(NSData*)serializedActionsResult
                            callID:(NSString*)callID NS_DESIGNATED_INITIALIZER;

// Convenience initializer for error responses without serialized protos.
// @param resultCode Status code returned by ActorService.
// @param errorMessage Optional failure description.
// @param callID Correlation call ID.
- (instancetype)initWithResultCode:(actor::mojom::ActionResultCode)resultCode
                      errorMessage:(NSString*)errorMessage
                            callID:(NSString*)callID;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_ACTUATION_DATA_TYPES_H_
