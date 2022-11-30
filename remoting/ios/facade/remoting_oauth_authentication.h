// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_FACADE_REMOTING_OAUTH_AUTHENTICATION_H_
#define REMOTING_IOS_FACADE_REMOTING_OAUTH_AUTHENTICATION_H_

#import "remoting/ios/facade/remoting_authentication.h"

// The OAuth implementation for RemotingAuthentication. Note that this is only
// used by the open source app variant. The official app uses SSO to sign in.
@interface RemotingOAuthAuthentication : NSObject<RemotingAuthentication>

// Provide an |authorizationCode| to authenticate a user as the first time user
// of the application or OAuth Flow.
- (void)authenticateWithAuthorizationCode:(NSString*)authorizationCode;

@end

#endif  // REMOTING_IOS_FACADE_REMOTING_OAUTH_AUTHENTICATION_H_
