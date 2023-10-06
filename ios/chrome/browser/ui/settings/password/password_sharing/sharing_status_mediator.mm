// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_mediator.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_consumer.h"

namespace {

const CGFloat kProfileImageSize = 60.0;

}

@implementation SharingStatusMediator {
  // Authentication Service to get the user's identity.
  raw_ptr<AuthenticationService> _authService;

  // Service that provides Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;
}

- (instancetype)initWithAuthService:(AuthenticationService*)authService
              accountManagerService:
                  (ChromeAccountManagerService*)accountManagerService {
  self = [super init];
  if (self) {
    _authService = authService;
    _accountManagerService = accountManagerService;
  }
  return self;
}

- (void)setConsumer:(id<SharingStatusConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setSenderImage:[self fetchSenderImage]];
}

#pragma mark - Private

// Fetches and returns sender's profile image from account manager service.
- (UIImage*)fetchSenderImage {
  id<SystemIdentity> identity =
      _authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (identity) {
    return CircularImageFromImage(
        _accountManagerService->GetIdentityAvatarWithIdentity(
            identity, IdentityAvatarSize::SmallSize),
        kProfileImageSize);
  }

  return DefaultSymbolTemplateWithPointSize(kPersonCropCircleSymbol,
                                            kProfileImageSize);
}

@end
