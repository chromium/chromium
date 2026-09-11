// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_actuation_data_types.h"

#import "components/optimization_guide/proto/features/actions_data.pb.h"

namespace {

// TODO(crbug.com/556739755): Extract SerializeProtoToNSData to a shared helper.
// Serializes a Protobuf message to NSData.
template <typename ProtoMessage>
NSData* SerializeProtoToNSData(const ProtoMessage& message) {
  std::string serialized;
  message.SerializeToString(&serialized);
  return [NSData dataWithBytes:serialized.data() length:serialized.size()];
}

}  // namespace

@implementation GeminiYieldAction

- (instancetype)initWithReason:(GeminiYieldReason)reason
                 messageToUser:(NSString*)messageToUser {
  self = [super init];
  if (self) {
    _reason = reason;
    _messageToUser = [messageToUser copy];
  }
  return self;
}

@end

@implementation GeminiActuationRequest

- (instancetype)initWithActionProtos:(NSArray<NSData*>*)actionProtos
                          taskUpdate:(NSString*)taskUpdate
                         yieldAction:(GeminiYieldAction*)yieldAction {
  self = [super init];
  if (self) {
    _actionProtos = [actionProtos copy];
    _taskUpdate = [taskUpdate copy];
    _yieldAction = yieldAction;
  }
  return self;
}

@end

@implementation GeminiActuationResponse

- (instancetype)initWithResultCode:(actor::mojom::ActionResultCode)resultCode
                      userResponse:(NSString*)userResponse
           serializedActionsResult:(NSData*)serializedActionsResult {
  self = [super init];
  if (self) {
    _resultCode = resultCode;
    _userResponse = [userResponse copy];
    _serializedActionsResult = [serializedActionsResult copy];
  }
  return self;
}

- (instancetype)initWithResultCode:(actor::mojom::ActionResultCode)resultCode
                      errorMessage:(const std::string&)errorMessage {
  optimization_guide::proto::ActionsResult actionsResult;
  actionsResult.set_action_result(static_cast<int32_t>(resultCode));
  actionsResult.set_error_message(errorMessage);
  NSData* failureData = SerializeProtoToNSData(actionsResult);
  return [self initWithResultCode:resultCode
                     userResponse:nil
          serializedActionsResult:failureData];
}

@end
