// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_ACCOUNT_ERROR_UI_INFO_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_ACCOUNT_ERROR_UI_INFO_H_

#import <Foundation/Foundation.h>

#import "components/sync/service/sync_service.h"

enum class AccountErrorUserActionableType {
  // No action to take.
  kNoAction,
  // User needs to enter their passphrase.
  kEnterPassphrase,
  // User needs to reauthenticate for the fetch keys.
  kReauthForFetchKeys,
  // User needs to reauthenticate for degraded recoverability.
  kReauthForDegradedRecoverability,
};

// Contains the information of the account error UI item.
@interface AccountErrorUIInfo : NSObject

- (instancetype)
     initWithErrorType:(syncer::SyncService::UserActionableError)errorType
    userActionableType:(AccountErrorUserActionableType)userActionableType
             messageID:(int)messageID
         buttonLabelID:(int)buttonLabelID NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Type of the error to display.
@property(nonatomic, assign, readonly)
    syncer::SyncService::UserActionableError errorType;

// Type of user actionable to resolve the error.
@property(nonatomic, assign, readonly)
    AccountErrorUserActionableType userActionableType;

// ID of the message localized string that gives details on the account error.
@property(nonatomic, assign, readonly) int messageID;

// ID of the button label localized string that gives details on the action to
// resolve the account error.
@property(nonatomic, assign, readonly) int buttonLabelID;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_ACCOUNT_ERROR_UI_INFO_H_
