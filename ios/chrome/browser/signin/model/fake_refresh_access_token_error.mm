// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/fake_refresh_access_token_error.h"

#import "base/apple/foundation_util.h"

@implementation FakeRefreshAccessTokenError

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity
                        callback:(HandleMDMNotificationCallback)callback {
  if ((self = [super init])) {
    CHECK(identity);
    CHECK(!callback.is_null());
    _identity = identity;
    _callback = callback;
  }
  return self;
}

#pragma mark - RefreshAccessTokenError

- (BOOL)isInvalidGrantError {
  return NO;
}

- (BOOL)isEqualToError:(id<RefreshAccessTokenError>)error {
  return self == error;
}

@end
