// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_REFRESH_ACCESS_TOKEN_ERROR_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_REFRESH_ACCESS_TOKEN_ERROR_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/signin/model/refresh_access_token_error.h"
#include "ios/chrome/browser/signin/model/system_identity_manager.h"

using HandleMDMNotificationCallback =
    base::RepeatingCallback<void(SystemIdentityManager::HandleMDMCallback)>;

// Fake implementation of RefreshAccessTokenError.
@interface FakeRefreshAccessTokenError : NSObject <RefreshAccessTokenError>

- (instancetype)initWithCallback:(HandleMDMNotificationCallback)callback
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Callback passed to the initializer.
@property(nonatomic, readonly) HandleMDMNotificationCallback callback;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_FAKE_REFRESH_ACCESS_TOKEN_ERROR_H_
