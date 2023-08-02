// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/sync/utils/account_error_ui_info.h"

@implementation AccountErrorUIInfo

- (instancetype)
     initWithErrorType:(syncer::SyncService::UserActionableError)errorType
    userActionableType:(AccountErrorUserActionableType)userActionableType
             messageID:(int)messageID
         buttonLabelID:(int)buttonLabelID {
  if (self = [super init]) {
    _errorType = errorType;
    _userActionableType = userActionableType;
    _messageID = messageID;
    _buttonLabelID = buttonLabelID;
  }
  return self;
}

@end
