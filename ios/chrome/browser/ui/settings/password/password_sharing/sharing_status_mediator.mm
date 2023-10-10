// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const CGFloat kProfileImageSize = 60.0;

}

@implementation SharingStatusMediator {
  // Authentication Service to get the user's identity.
  raw_ptr<AuthenticationService> _authService;

  // Service that provides Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;

  // Contains information about the recipients that the user selected to share a
  // password with.
  NSArray<RecipientInfoForIOSDisplay*>* _recipients;
}

- (instancetype)
      initWithAuthService:(AuthenticationService*)authService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
               recipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients {
  self = [super init];
  if (self) {
    _authService = authService;
    _accountManagerService = accountManagerService;
    _recipients = recipients;
  }
  return self;
}

- (void)setConsumer:(id<SharingStatusConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setSenderImage:[self fetchSenderImage]];
  [_consumer setSubtitleString:[self subtitleString]];
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

- (NSString*)subtitleString {
  // TODO(crbug.com/1463882): Add passing link to the site.
  if (_recipients.count == 1) {
    return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
        IDS_IOS_PASSWORD_SHARING_SUCCESS_SUBTITLE,
        base::SysNSStringToUTF16(_recipients[0].fullName), u""));
  }

  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_IOS_PASSWORD_SHARING_SUCCESS_SUBTITLE_MULTIPLE_RECIPIENTS, u""));
}

@end
