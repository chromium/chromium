// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_view_controller.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/ui/authentication/authentication_constants.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PostRestoreSignInViewController ()

// Returns the given name of the last account that was signed in pre-restore.
@property(nonatomic, copy) NSString* userGivenName;

// Returns the full name of the last account that was signed in pre-restore.
@property(nonatomic, copy) NSString* userFullName;

// Returns the email address of the last account that was signed in
// pre-restore.
@property(nonatomic, copy) NSString* userEmail;

// Contains the avatar of the last account that was signed in pre-restore.
@property(nonatomic, strong) UIImage* userAvatar;

// Button controlling the display of the selected identity.
@property(nonatomic, strong) IdentityButtonControl* identityControl;

@end

@implementation PostRestoreSignInViewController {
  std::optional<AccountInfo> _accountInfo;
}

#pragma mark - Initialization

- (instancetype)initWithAccountInfo:(AccountInfo)accountInfo {
  if ((self = [super init])) {
    _accountInfo = accountInfo;
    if (_accountInfo.has_value()) {
      _userEmail = base::SysUTF8ToNSString(_accountInfo->email);
      _userGivenName = base::SysUTF8ToNSString(_accountInfo->given_name);
      _userFullName = base::SysUTF8ToNSString(_accountInfo->full_name);
      if (_accountInfo->account_image.IsEmpty()) {
        _userAvatar = ios::provider::GetSigninDefaultAvatar();
      } else {
        _userAvatar = _accountInfo->account_image.ToUIImage();
      }
    }
  }

  return self;
}

#pragma mark - Public

- (void)loadView {
#if BUILDFLAG(IOS_USE_BRANDED_SYMBOLS)
  self.bannerName = kChromeSigninBannerImage;
#else
  self.bannerName = kChromiumSigninBannerImage;
#endif

  if (self.userGivenName.length > 0) {
    self.titleText = l10n_util::GetNSStringF(
        IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PROMO_TITLE,
        base::SysNSStringToUTF16(self.userGivenName));
    self.primaryActionString = l10n_util::GetNSStringF(
        IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PRIMARY_ACTION,
        base::SysNSStringToUTF16(self.userGivenName));
  } else {
    self.titleText = l10n_util::GetNSString(
        IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PROMO_TITLE_SHORT);
    self.primaryActionString = l10n_util::GetNSString(
        IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PRIMARY_ACTION_SHORT);
  }
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_SECONDARY_ACTION);

  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    self.disclaimerText = l10n_util::GetNSString(
        IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_NOTE_IPAD);
  } else {
    self.disclaimerText = l10n_util::GetNSString(
        IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_NOTE_IPHONE);
  }

  // This view does not have a subtitle, so a bottom margin is not needed.
  self.subtitleBottomMargin = 0;

  // Set up the identity control to be centered horizontally, and at the to of
  // the specificContentView.
  [self.specificContentView addSubview:self.identityControl];
  [NSLayoutConstraint activateConstraints:@[
    [self.identityControl.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [self.identityControl.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [self.identityControl.widthAnchor
        constraintEqualToAnchor:self.specificContentView.widthAnchor],
    [self.identityControl.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .bottomAnchor],
  ]];

  if (_userFullName != nil && _userEmail != nil)
    [self.identityControl setIdentityName:_userFullName email:_userEmail];
  if (_userAvatar != nil)
    [self.identityControl
        setIdentityAvatar:CircularImageFromImage(
                              ios::provider::GetSigninDefaultAvatar(),
                              kAccountProfilePhotoDimension)];

  [super loadView];
}

#pragma mark - Properties

- (IdentityButtonControl*)identityControl {
  if (!_identityControl) {
    _identityControl = [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
    _identityControl.translatesAutoresizingMaskIntoConstraints = NO;
    [_identityControl addTarget:self
                         action:@selector(identityButtonControlTapped:forEvent:)
               forControlEvents:UIControlEventTouchUpInside];

    // Setting the content hugging priority isn't working, so creating a
    // low-priority constraint to make sure that the view is as small as
    // possible.
    NSLayoutConstraint* heightConstraint =
        [_identityControl.heightAnchor constraintEqualToConstant:0];
    heightConstraint.priority = UILayoutPriorityDefaultLow - 1;
    heightConstraint.active = YES;
  }
  return _identityControl;
}

#pragma mark - internal

- (void)identityButtonControlTapped:(id)sender forEvent:(UIEvent*)event {
}

@end
