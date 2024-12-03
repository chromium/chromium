// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/managed_profile_creation/managed_profile_creation_view_controller.h"

#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@interface ManagedProfileCreationViewController ()
@end

@implementation ManagedProfileCreationViewController {
  NSString* _userEmail;
  NSString* _hostedDomain;
}

- (instancetype)initWithUserEmail:(NSString*)userEmail
                     hostedDomain:(NSString*)hostedDomain {
  self = [super init];
  if (self) {
    _userEmail = userEmail;
    _hostedDomain = hostedDomain;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view.accessibilityIdentifier =
      kManagedProfileCreationScreenAccessibilityIdentifier;
  self.bannerSize = BannerImageSizeType::kStandard;
  self.scrollToEndMandatory = YES;
  self.readMoreString =
      l10n_util::GetNSString(IDS_IOS_FIRST_RUN_SCREEN_READ_MORE);

  // Set banner.
  self.bannerName = kEnterpriseSigninBannerSymbol;

  self.titleText =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_TITLE);
  self.subtitleText =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_SUBTITLE);

  self.disclaimerText = l10n_util::GetNSStringF(
      IDS_IOS_ENTERPRISE_PROFILE_CREATION_ACCOUNT_MANAGEMENT_DISCLAIMER,
      base::SysNSStringToUTF16(_userEmail),
      base::SysNSStringToUTF16(_hostedDomain));

  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_CONTINUE);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_ENTERPRISE_PROFILE_CREATION_CANCEL);

  // Call super after setting up the strings and others, as required per super
  // class.
  [super viewDidLoad];
}

@end
