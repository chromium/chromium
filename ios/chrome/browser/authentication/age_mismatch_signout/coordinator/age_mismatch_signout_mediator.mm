// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/age_mismatch_signout/coordinator/age_mismatch_signout_mediator.h"

#import "base/memory/raw_ptr.h"
#import "ios/chrome/browser/authentication/age_mismatch_signout/ui/age_mismatch_signout_consumer.h"
#import "ios/chrome/browser/signin/model/avatar/avatar_provider.h"
#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@implementation AgeMismatchSignoutMediator {
  id<SystemIdentity> _identity;
  raw_ptr<signin::AvatarProvider> _identityAvatarProvider;
}

- (instancetype)initWithIdentity:(id<SystemIdentity>)identity
          identityAvatarProvider:
              (signin::AvatarProvider*)identityAvatarProvider {
  self = [super init];
  if (self) {
    _identity = identity;
    _identityAvatarProvider = identityAvatarProvider;
  }
  return self;
}

- (void)setConsumer:(id<AgeMismatchSignoutConsumer>)consumer {
  _consumer = consumer;
  if (_consumer) {
    [self updateConsumer];
  }
}

- (void)disconnect {
  _identity = nil;
  _identityAvatarProvider = nullptr;
  _consumer = nil;
}

#pragma mark - Private

- (void)updateConsumer {
  if (!_identity) {
    return;
  }

  NSString* name = _identity.userFullName;
  NSString* email = _identity.userEmail;
  UIImage* avatar = _identityAvatarProvider->GetIdentityAvatar(
      _identity, IdentityAvatarSize::Regular);

  [self.consumer setPrimaryIdentityName:name
                                  email:email
                                 avatar:avatar
                                managed:NO];
}

@end
