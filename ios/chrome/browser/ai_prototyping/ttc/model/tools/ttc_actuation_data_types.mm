// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"

@implementation TTCActuationRequest

#pragma mark - Public

- (instancetype)initWithActionProtos:(NSArray<NSData*>*)actionProtos
                          taskUpdate:(NSString*)taskUpdate
                             callIDs:(NSArray<NSString*>*)callIDs {
  self = [super init];
  if (self) {
    _actionProtos = [actionProtos copy] ?: @[];
    _taskUpdate = [taskUpdate copy];
    _callIDs = [callIDs copy] ?: @[];
  }
  return self;
}

- (instancetype)initWithActionProtos:(NSArray<NSData*>*)actionProtos
                          taskUpdate:(NSString*)taskUpdate
                              callID:(NSString*)callID {
  NSArray<NSString*>* callIDs = callID ? @[ callID ] : @[];
  return [self initWithActionProtos:actionProtos
                         taskUpdate:taskUpdate
                            callIDs:callIDs];
}

- (NSString*)callID {
  return _callIDs.firstObject ?: @"";
}

@end

@implementation TTCActuationResponse

#pragma mark - Public

- (instancetype)initWithResultCode:(actor::mojom::ActionResultCode)resultCode
                      errorMessage:(NSString*)errorMessage
           serializedActionsResult:(NSData*)serializedActionsResult
                            callID:(NSString*)callID {
  self = [super init];
  if (self) {
    _resultCode = resultCode;
    _errorMessage = [errorMessage copy];
    _serializedActionsResult = [serializedActionsResult copy];
    _callID = [callID copy];
  }
  return self;
}

- (instancetype)initWithResultCode:(actor::mojom::ActionResultCode)resultCode
                      errorMessage:(NSString*)errorMessage
                            callID:(NSString*)callID {
  return [self initWithResultCode:resultCode
                     errorMessage:errorMessage
          serializedActionsResult:nil
                           callID:callID];
}

@end
