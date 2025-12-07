// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/common/credential_provider/ui/passkey_welcome_screen_strings.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
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

  // Delegate for this view controller.
  __weak id<PasskeyWelcomeScreenViewControllerDelegate>
      _passkeyWelcomeScreenViewControllerDelegate;

  // The block that should be executed when the primary button is tapped.
  ProceduralBlock _primaryButtonAction;

  // Contains all the strings that need to be displayed in the view.
  PasskeyWelcomeScreenStrings* _strings;
}

- (instancetype)initForPurpose:(PasskeyWelcomeScreenPurpose)purpose
       navigationItemTitleView:(UIView*)navigationItemTitleView
                      delegate:(id<PasskeyWelcomeScreenViewControllerDelegate>)
                                   delegate
           primaryButtonAction:(ProceduralBlock)primaryButtonAction
                       strings:(PasskeyWelcomeScreenStrings*)strings {
  self = [super initWithTaskRunner:nullptr];
  if (self) {
    _purpose = purpose;
    _navigationItemTitleView = navigationItemTitleView;
    _passkeyWelcomeScreenViewControllerDelegate = delegate;
    _primaryButtonAction = primaryButtonAction;
    _strings = strings;
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.bannerName = GetBannerName(_purpose);
  self.bannerSize = BannerImageSizeType::kExtraShort;

  self.titleText = _strings.title;
  self.titleTopMarginWhenNoHeaderImage = kTitleHorizontalAndTopMargin;
  self.titleHorizontalMargin = kTitleHorizontalAndTopMargin;

  if (_purpose == PasskeyWelcomeScreenPurpose::kEnroll) {
    self.specificContentView = [self createSpecificContentView];
  } else {
    self.subtitleText = _strings.subtitle;
  }

  self.configuration.primaryActionString = _strings.primaryButton;
  self.configuration.secondaryActionString = _strings.secondaryButton;

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

  InstructionView* instructionView =
      [[InstructionView alloc] initWithList:_strings.instructions];
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;
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
  footerMessage.text = _strings.footer;

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
