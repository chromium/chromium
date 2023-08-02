// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/domain/user_info.h"

@implementation UserInfo

@synthesize userId = _userId;
@synthesize userFullName = _userFullName;
@synthesize userEmail = _userEmail;
@synthesize refreshToken = _refreshToken;

- (BOOL)isAuthenticated {
  if (_userEmail && _userEmail.length > 0 && _refreshToken &&
      _refreshToken.length > 0) {
    return YES;
  }
  return NO;
}

- (NSComparisonResult)compare:(UserInfo*)user {
  return [self.userId compare:user.userId];
}

- (NSString*)description {
  return [NSString stringWithFormat:@"UserInfo: userEmail=%@ refreshToken=%@",
                                    _userEmail, _refreshToken];
}

@end
