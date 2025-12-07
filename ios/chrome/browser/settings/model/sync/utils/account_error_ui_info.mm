// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/model/sync/utils/account_error_ui_info.h"

@implementation AccountErrorUIInfo

- (instancetype)
     initWithErrorType:(syncer::SyncService::UserActionableError)errorType
    userActionableType:(AccountErrorUserActionableType)userActionableType
             messageID:(int)messageID
         buttonLabelID:(int)buttonLabelID {
  if ((self = [super init])) {
    _errorType = errorType;
    _userActionableType = userActionableType;
    _messageID = messageID;
    _buttonLabelID = buttonLabelID;
  }
  return self;
}

- (BOOL)isEqual:(id)other {
  if (other == self) {
    return YES;
  }
  if (!other || ![other isKindOfClass:[self class]]) {
    return NO;
  }
  return [self isEqualToAccountErrorUIInfo:other];
}

- (BOOL)isEqualToAccountErrorUIInfo:(AccountErrorUIInfo*)info {
  if (self == info) {
    return YES;
  }
  return _errorType == info.errorType &&
         _userActionableType == info.userActionableType &&
         _messageID == info.messageID && _buttonLabelID == info.buttonLabelID;
}

- (NSUInteger)hash {
  return [@[
    @(static_cast<int>(_errorType)), @(static_cast<int>(_userActionableType)),
    @(_messageID), @(_buttonLabelID)
  ] hash];
}
@end
