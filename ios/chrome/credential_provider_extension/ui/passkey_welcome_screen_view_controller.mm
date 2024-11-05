// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/passkey_welcome_screen_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Leading, trailing and top margin to use for the screen's title.
constexpr CGFloat kTitleHorizontalAndTopMargin = 24;

// Returns the background color for this view.
UIColor* GetBackgroundColor() {
  return [UIColor colorNamed:kPrimaryBackgroundColor];
}

// Returns the banner name to use depending on the provided `purpose`.
NSString* GetBannerName(PasskeyWelcomeScreenPurpose purpose) {
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      return @"passkey_generic_banner";
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      return @"passkey_bootstrapping_banner";
  }
}

// Returns the title to use depending on the provided `purpose`.
NSString* GetTitleString(PasskeyWelcomeScreenPurpose purpose) {
  NSString* stringID;
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_TITLE";
      break;
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      stringID =
          @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_PARTIAL_BOOTSRAPPING_TITLE";
      break;
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_BOOTSRAPPING_TITLE";
      break;
  }
  return NSLocalizedString(stringID, @"The title of the welcome screen.");
}

// Returns the subtitle to use depending on the provided `purpose`.
NSString* GetSubtitleString(PasskeyWelcomeScreenPurpose purpose) {
  NSString* stringID;
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
      NOTREACHED_NORETURN();
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      stringID =
          @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_PARTIAL_BOOTSRAPPING_SUBTITLE";
      break;
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_BOOTSRAPPING_SUBTITLE";
      break;
  }
  return NSLocalizedString(stringID, @"The subtitle of the welcome screen.");
}

// Returns the title to use for the primary button depending on the provided
// `purpose`.
NSString* GetPrimaryButtonTitle(PasskeyWelcomeScreenPurpose purpose) {
  NSString* stringID;
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_GET_STARTED_BUTTON";
      break;
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      stringID = @"IDS_IOS_CREDENTIAL_PROVIDER_NEXT_BUTTON";
      break;
  }
  return NSLocalizedString(
      stringID, @"The title of the welcome screen's primary button.");
}

}  // namespace

@interface PasskeyWelcomeScreenViewController () <
    PromoStyleViewControllerDelegate>

@end

@implementation PasskeyWelcomeScreenViewController {
  // The purpose for which this view is shown. Used to appropriately set up the
  // UI elements.
  PasskeyWelcomeScreenPurpose _purpose;

  // The block that should be executed when the primary button is tapped.
  ProceduralBlock _primaryButtonAction;
}

- (instancetype)initForPurpose:(PasskeyWelcomeScreenPurpose)purpose
           primaryButtonAction:(ProceduralBlock)primaryButtonAction {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _purpose = purpose;
    _primaryButtonAction = primaryButtonAction;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.bannerName = GetBannerName(_purpose);
  self.bannerSize = BannerImageSizeType::kExtraShort;

  self.titleText = GetTitleString(_purpose);
  self.titleTopMarginWhenNoHeaderImage = kTitleHorizontalAndTopMargin;
  self.titleHorizontalMargin = kTitleHorizontalAndTopMargin;

  if (_purpose == PasskeyWelcomeScreenPurpose::kEnroll) {
    // TODO(crbug.com/355042392): Set up `self.specificContentView`.
  } else {
    self.subtitleText = GetSubtitleString(_purpose);
  }

  self.primaryActionString = GetPrimaryButtonTitle(_purpose);
  self.secondaryActionString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NOT_NOW_BUTTON",
                        @"The title of the welcome screen's secondary button.");

  [super viewDidLoad];

  self.view.backgroundColor = GetBackgroundColor();
}

#pragma mark - PromoStyleViewController

- (UIFontTextStyle)titleLabelFontTextStyle {
  return UIFontTextStyleTitle1;
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  ProceduralBlock primaryButtonAction = _primaryButtonAction;
  _primaryButtonAction = nil;

  primaryButtonAction();
}

- (void)didTapSecondaryActionButton {
  // TODO(crbug.com/355042392): Handle taps on "Not now" button.
}

@end
