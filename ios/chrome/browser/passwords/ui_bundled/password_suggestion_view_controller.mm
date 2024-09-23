// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/password_suggestion_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/common/password_generation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/ui_bundled/password_constants.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util.h"

@interface PasswordSuggestionViewController ()

// The current user's email.
@property(nonatomic, copy) NSString* userEmail;

// The suggested strong password.
@property(nonatomic, copy) NSString* passwordSuggestion;

@end

namespace {
constexpr CGFloat customSpacingBeforeImageIfNoNavigationBar = 24;
constexpr CGFloat customSpacingAfterImage = 1;
}  // namespace

@implementation PasswordSuggestionViewController {
  // YES when the bottom sheet is proactive where it is triggered upon focus.
  BOOL _proactive;
}

#pragma mark - Public

- (instancetype)initWithPasswordSuggestion:(NSString*)passwordSuggestion
                                 userEmail:(NSString*)userEmail
                                 proactive:(BOOL)proactivePasswordGeneration {
  if ((self = [super init])) {
    _userEmail = userEmail;
    _passwordSuggestion = passwordSuggestion;
    _proactive = proactivePasswordGeneration;
  }
  return self;
}

- (void)viewDidLoad {
  self.image = ios::provider::GetBrandedImage(
      ios::provider::BrandedImage::kPasswordSuggestionKey);
  self.imageHasFixedSize = YES;
  self.showDismissBarButton = NO;
  self.customSpacingBeforeImageIfNoNavigationBar =
      customSpacingBeforeImageIfNoNavigationBar;
  self.customSpacingAfterImage = customSpacingAfterImage;
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.topAlignedLayout = YES;

  self.titleString = l10n_util::GetNSString(
      _passwordSuggestion.length >=
              autofill::password_generation::kLengthSufficientForStrongLabel
          ? IDS_IOS_SUGGESTED_STRONG_PASSWORD
          : IDS_IOS_SUGGESTED_PASSWORD);
  self.secondaryTitleString = self.passwordSuggestion;
  self.subtitleString = l10n_util::GetNSStringF(
      IDS_IOS_SUGGESTED_STRONG_PASSWORD_HINT_DISPLAYING_EMAIL,
      base::SysNSStringToUTF16(self.userEmail));
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_USE_SUGGESTED_STRONG_PASSWORD);
  if (_proactive) {
    self.secondaryActionString =
        l10n_util::GetNSString(IDS_IOS_PASSWORD_BOTTOM_SHEET_USE_KEYBOARD);
    self.secondaryActionImage =
        DefaultSymbolWithPointSize(kKeyboardSymbol, kSymbolActionPointSize);
  } else {
    self.secondaryActionString = l10n_util::GetNSString(IDS_CANCEL);
  }

  [super viewDidLoad];

  self.view.accessibilityIdentifier =
      kPasswordProtectionViewAccessibilityIdentifier;
}

@end
