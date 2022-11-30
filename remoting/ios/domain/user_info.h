// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_DOMAIN_USER_INFO_H_
#define REMOTING_IOS_DOMAIN_USER_INFO_H_

#import <Foundation/Foundation.h>

// A detail record for a Remoting User.
@interface UserInfo : NSObject

@property(nonatomic, copy) NSString* userId;
@property(nonatomic, copy) NSString* userFullName;
@property(nonatomic, copy) NSString* userEmail;
// TODO(yuweih): SSO doesn't use a refresh token and it should not be used to
// decide whether the UserInfo is authenticated.
@property(nonatomic, copy) NSString* refreshToken;

// This returns the authenticated state of the this user info object.
- (BOOL)isAuthenticated;
// Compare two |UserInfo| objects.
- (NSComparisonResult)compare:(UserInfo*)user;

@end

#endif  //  REMOTING_IOS_DOMAIN_USER_INFO_H_
