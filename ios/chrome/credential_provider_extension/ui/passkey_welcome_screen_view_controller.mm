// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/passkey_welcome_screen_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/instruction_view/instruction_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Vertical spacing between the UI elements contained in the
// `specificContentView`.
constexpr CGFloat kSpecificContentVerticalSpacing = 24;

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
      NOTREACHED();
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

  // The view to be used as the navigation bar title view.
  UIView* _navigationItemTitleView;

  // Email address associated with the signed in account. Depending on the
  // PasskeyWelcomeScreenPurpose, the user email might or might no have to be
  // dispalyed in the UI. If part of the UI, the `userEmail` must not be `nil`.
  NSString* _userEmail;

  // Delegate for this view controller.
  __weak id<PasskeyWelcomeScreenViewControllerDelegate>
      _passkeyWelcomeScreenViewControllerDelegate;

  // The block that should be executed when the primary button is tapped.
  ProceduralBlock _primaryButtonAction;
}

- (instancetype)initForPurpose:(PasskeyWelcomeScreenPurpose)purpose
       navigationItemTitleView:(UIView*)navigationItemTitleView
                     userEmail:(NSString*)userEmail
                      delegate:(id<PasskeyWelcomeScreenViewControllerDelegate>)
                                   delegate
           primaryButtonAction:(ProceduralBlock)primaryButtonAction {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _purpose = purpose;
    _navigationItemTitleView = navigationItemTitleView;
    _userEmail = userEmail;
    _passkeyWelcomeScreenViewControllerDelegate = delegate;
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
    self.specificContentView = [self createSpecificContentView];
  } else {
    self.subtitleText = GetSubtitleString(_purpose);
  }

  self.primaryActionString = GetPrimaryButtonTitle(_purpose);
  self.secondaryActionString =
      NSLocalizedString(@"IDS_IOS_CREDENTIAL_PROVIDER_NOT_NOW_BUTTON",
                        @"The title of the welcome screen's secondary button.");

  [super viewDidLoad];

  self.view.backgroundColor = GetBackgroundColor();
  self.navigationItem.titleView = _navigationItemTitleView;
}

#pragma mark - PromoStyleViewController

- (UIFontTextStyle)titleLabelFontTextStyle {
  return UIFontTextStyleTitle1;
}

#pragma mark - PromoStyleViewControllerDelegate

// Creates and configures the screen-specific view that's placed between the
// titles and buttons.
- (UIView*)createSpecificContentView {
  UIView* specificContentView = [[UIView alloc] init];
  specificContentView.translatesAutoresizingMaskIntoConstraints = NO;

  InstructionView* instructionView = [self createInstructionView];
  [specificContentView addSubview:instructionView];
  AddSameConstraintsToSides(
      instructionView, specificContentView,
      LayoutSides::kTop | LayoutSides::kLeading | LayoutSides::kTrailing);

  UILabel* footerMessage = [self createFooterMessage];
  [specificContentView addSubview:footerMessage];
  AddSameConstraintsToSides(
      footerMessage, specificContentView,
      LayoutSides::kLeading | LayoutSides::kTrailing | LayoutSides::kBottom);

  [NSLayoutConstraint activateConstraints:@[
    [footerMessage.topAnchor
        constraintGreaterThanOrEqualToAnchor:instructionView.bottomAnchor
                                    constant:kSpecificContentVerticalSpacing],
  ]];

  return specificContentView;
}

// Creates and configures the instruction view for the enrollment welcome
// screen.
- (InstructionView*)createInstructionView {
  NSArray<NSString*>* steps = @[
    NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_1",
        @"First step of the passkey enrollment instructions"),
    NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_2",
        @"First step of the passkey enrollment instructions"),
    NSLocalizedString(
        @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_INSTRUCTIONS_STEP_3",
        @"First step of the passkey enrollment instructions"),
  ];

  InstructionView* instructionView =
      [[InstructionView alloc] initWithList:steps];
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;

  return instructionView;
}

// Creates and configures the footer message for the enrollment welcome screen.
- (UILabel*)createFooterMessage {
  UILabel* footerMessage = [[UILabel alloc] init];
  footerMessage.translatesAutoresizingMaskIntoConstraints = NO;
  footerMessage.textAlignment = NSTextAlignmentCenter;
  footerMessage.adjustsFontForContentSizeCategory = YES;
  footerMessage.textColor = [UIColor colorNamed:kGrey600Color];
  footerMessage.numberOfLines = 0;

  UIFont* font = [UIFont systemFontOfSize:13 weight:UIFontWeightRegular];
  footerMessage.font = [[UIFontMetrics defaultMetrics] scaledFontForFont:font];

  CHECK(_userEmail);
  NSString* stringWithPlaceholder = NSLocalizedString(
      @"IDS_IOS_CREDENTIAL_PROVIDER_PASSKEY_ENROLLMENT_FOOTER_MESSAGE",
      @"Footer messsage shown at the bottom of the screen-specific view.");
  footerMessage.text =
      [stringWithPlaceholder stringByReplacingOccurrencesOfString:@"$1"
                                                       withString:_userEmail];

  return footerMessage;
}

- (void)didTapPrimaryActionButton {
  if (self.navigationController.topViewController != self) {
    return;
  }

  CHECK(_primaryButtonAction);
  _primaryButtonAction();
}

- (void)didTapSecondaryActionButton {
  [_passkeyWelcomeScreenViewControllerDelegate
      passkeyWelcomeScreenViewControllerShouldBeDismissed:self];
}

@end
