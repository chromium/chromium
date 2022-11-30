// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_suggestion_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/passwords/password_constants.h"
#import "ios/chrome/grit/ios_google_chrome_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordSuggestionViewController ()

// The current user's email.
@property(nonatomic, copy) NSString* userEmail;

// The suggested strong password.
@property(nonatomic, copy) NSString* passwordSuggestion;

@end

namespace {
constexpr CGFloat customSpacingBeforeImageIfNoToolbar = 24;
constexpr CGFloat customSpacingAfterImage = 1;
}  // namespace

@implementation PasswordSuggestionViewController

#pragma mark - Public

- (instancetype)initWithPasswordSuggestion:(NSString*)passwordSuggestion
                                 userEmail:(NSString*)userEmail {
  if (self = [super initWithNibName:nil bundle:nil]) {
    _userEmail = userEmail;
    _passwordSuggestion = passwordSuggestion;
  }
  return self;
}

- (void)viewDidLoad {
  self.image = ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kPasswordSuggestionKey);
  self.imageHasFixedSize = YES;
  self.showDismissBarButton = NO;
  self.customSpacingBeforeImageIfNoToolbar =
      customSpacingBeforeImageIfNoToolbar;
  self.customSpacingAfterImage = customSpacingAfterImage;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;

  self.titleString = l10n_util::GetNSString(IDS_IOS_SUGGESTED_STRONG_PASSWORD);
  self.secondaryTitleString = self.passwordSuggestion;
  self.subtitleString = l10n_util::GetNSStringF(
      IDS_IOS_SUGGESTED_STRONG_PASSWORD_HINT_DISPLAYING_EMAIL,
      base::SysNSStringToUTF16(self.userEmail));
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_USE_SUGGESTED_STRONG_PASSWORD);
  self.secondaryActionString = l10n_util::GetNSString(IDS_CANCEL);

  [super viewDidLoad];

  self.view.accessibilityIdentifier =
      kPasswordProtectionViewAccessibilityIdentifier;
}

@end
