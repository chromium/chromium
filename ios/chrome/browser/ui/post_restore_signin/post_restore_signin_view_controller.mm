// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

#import <Foundation/Foundation.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PostRestoreSignInViewController ()

// The current user's given name.
@property(nonatomic, assign) NSString* userGivenName;

@end

@implementation PostRestoreSignInViewController

#pragma mark - Initialization

- (instancetype)initWithUserGivenName:(NSString*)userGivenName {
  if (self = [super init])
    _userGivenName = userGivenName;

  return self;
}

#pragma mark - Public

- (void)loadView {
  self.bannerName = @"signin_banner";

  self.titleText = l10n_util::GetNSStringF(
      IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PROMO_TITLE,
      base::SysNSStringToUTF16(self.userGivenName));
  self.primaryActionString = l10n_util::GetNSStringF(
      IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_PRIMARY_ACTION,
      base::SysNSStringToUTF16(self.userGivenName));
  self.secondaryActionString = l10n_util::GetNSString(
      IDS_IOS_POST_RESTORE_SIGN_IN_FULLSCREEN_SECONDARY_ACTION);

  [super loadView];
}

@end
