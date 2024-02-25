// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/multi_avatar_image_util.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_consumer.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

namespace {

const CGFloat kProfileImageSize = 60.0;

}

@implementation SharingStatusMediator {
  // Authentication Service to get the user's identity.
  raw_ptr<AuthenticationService> _authService;

  // Service that provides Chrome identities.
  raw_ptr<ChromeAccountManagerService> _accountManagerService;

  // Used to fetch favicon images.
  raw_ptr<FaviconLoader> _faviconLoader;

  // Contains information about the recipients that the user selected to share a
  // password with.
  NSArray<RecipientInfoForIOSDisplay*>* _recipients;

  // Website for which the password is being shared.
  NSString* _website;

  // Url of the site for which the password is being shared.
  GURL _URL;

  // Url which allows to change the password that is being shared. Can be null
  // for Android app credentials.
  std::optional<GURL> _changePasswordURL;
}

- (instancetype)
      initWithAuthService:(AuthenticationService*)authService
    accountManagerService:(ChromeAccountManagerService*)accountManagerService
            faviconLoader:(FaviconLoader*)faviconLoader
               recipients:(NSArray<RecipientInfoForIOSDisplay*>*)recipients
                  website:(NSString*)website
                      URL:(const GURL&)URL
        changePasswordURL:(const std::optional<GURL>&)changePasswordURL {
  self = [super init];
  if (self) {
    _authService = authService;
    _accountManagerService = accountManagerService;
    _faviconLoader = faviconLoader;
    _recipients = recipients;
    _website = website;
    _URL = URL;
    _changePasswordURL = changePasswordURL;
  }
  return self;
}

- (void)setConsumer:(id<SharingStatusConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }

  _consumer = consumer;
  [_consumer setSenderImage:[self fetchSenderImage]];
  [_consumer setRecipientImage:[self createRecipientImage]];
  [_consumer setSubtitleString:[self subtitleString]];
  [_consumer setFooterString:[self footerString]];
  [_consumer setURL:_URL];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  _faviconLoader->FaviconForPageUrl(
      URL.gurl, kDesiredSmallFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/false, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
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

// Creates a multi-avatar image of recipients from their profile images.
- (UIImage*)createRecipientImage {
  NSMutableArray<UIImage*>* recipientImages = [NSMutableArray array];
  for (RecipientInfoForIOSDisplay* recipient : _recipients) {
    [recipientImages addObject:recipient.profileImage];
  }

  return CreateMultiAvatarImage(recipientImages, kProfileImageSize);
}

// Creates subtitle string based on the amount of recipients chosen for sharing.
// For one recipient the subtitle contains the name of that recipients, whereas
// for multiple recipients it is replaced with more generic string.
- (NSString*)subtitleString {
  if (_recipients.count == 1) {
    return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
        IDS_IOS_PASSWORD_SHARING_SUCCESS_SUBTITLE,
        base::SysNSStringToUTF16(_recipients[0].fullName),
        base::SysNSStringToUTF16(_website)));
  }

  return base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
      IDS_IOS_PASSWORD_SHARING_SUCCESS_SUBTITLE_MULTIPLE_RECIPIENTS,
      base::SysNSStringToUTF16(_website)));
}

// Creates footer string informing the user how to revoke sharing access.
- (NSString*)footerString {
  return _changePasswordURL.has_value()
             ? base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
                   IDS_IOS_PASSWORD_SHARING_SUCCESS_FOOTNOTE,
                   base::SysNSStringToUTF16(_website)))
             : base::SysUTF16ToNSString(l10n_util::GetStringFUTF16(
                   IDS_IOS_PASSWORD_SHARING_SUCCESS_FOOTNOTE_ANDROID_APP,
                   base::SysNSStringToUTF16(_website)));
}

@end
