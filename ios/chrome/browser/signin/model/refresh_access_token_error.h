// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_REFRESH_ACCESS_TOKEN_ERROR_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_REFRESH_ACCESS_TOKEN_ERROR_H_

#import <Foundation/Foundation.h>

// Stores information about error that happens during refreshing the
// access token for a given identity.
@protocol RefreshAccessTokenError

// Is the access token refresh failure due to an invalid grant error.
@property(nonatomic, readonly) BOOL isInvalidGrantError;

// Returns whether `error` is identical to `self`.
- (BOOL)isEqualToError:(id<RefreshAccessTokenError>)error;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_REFRESH_ACCESS_TOKEN_ERROR_H_
