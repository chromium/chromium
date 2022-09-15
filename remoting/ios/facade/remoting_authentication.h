// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_REMOTING_AUTHENTICATION_H_
#define REMOTING_IOS_FACADE_REMOTING_AUTHENTICATION_H_

#import "remoting/ios/domain/user_info.h"

#include "base/memory/weak_ptr.h"

typedef NS_ENUM(NSInteger, RemotingAuthenticationStatus) {
  RemotingAuthenticationStatusSuccess,
  RemotingAuthenticationStatusNetworkError,
  RemotingAuthenticationStatusAuthError
};

typedef void (^AccessTokenCallback)(RemotingAuthenticationStatus status,
                                    NSString* userEmail,
                                    NSString* accessToken);

// |RemotingAuthenticationDelegate|s are interested in authentication related
// notifications.
@protocol RemotingAuthenticationDelegate<NSObject>

// Notifies the delegate that the user has been updated.
- (void)userDidUpdate:(UserInfo*)user;

@end

// This is the interface that will manage the details around authentication
// management and currently active user. It will make sure the user object is
// saved to the keychain correctly and loaded on startup. It also is the entry
// point for gaining access to an auth token for authorized calls.
// TODO(yuweih): Refactor and rewrite this class in C++.
@protocol RemotingAuthentication<NSObject>

// Fetches an Access Token and passes it back to the callback if the user is
// authenticated. Otherwise does nothing.
// TODO(nicholss): We might want to throw an error or add error message to
// the callback sig to be able to react to the un-authed case.
- (void)callbackWithAccessToken:(AccessTokenCallback)onAccessToken;

// Forget the current user.
- (void)logout;

// Invalidates the cached access token.
- (void)invalidateCache;

// Returns the currently logged in user or nil.
@property(strong, nonatomic, readonly) UserInfo* user;

// Delegate receives updates on user changes.
@property(weak, nonatomic) id<RemotingAuthenticationDelegate> delegate;

@end

#endif  // REMOTING_IOS_FACADE_REMOTING_AUTHENTICATION_H_
