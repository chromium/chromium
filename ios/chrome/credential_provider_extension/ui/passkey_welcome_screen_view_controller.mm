// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/passkey_welcome_screen_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Returns the background color for this view.
UIColor* BackgroundColor() {
  return [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
}

// Returns the title to use for the primary button.
NSString* PrimaryButtonTitle(PasskeyWelcomeScreenPurpose purpose) {
  switch (purpose) {
    case PasskeyWelcomeScreenPurpose::kEnroll:
      return @"Tap to enroll";
    case PasskeyWelcomeScreenPurpose::kFixDegradedRecoverability:
      return @"Tap to fix degraded recoverability";
    case PasskeyWelcomeScreenPurpose::kReauthenticate:
      return @"Tap to reauth";
  }
}

}  // namespace

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
  [super viewDidLoad];

  self.view.backgroundColor = BackgroundColor();

  UIButton* primaryButton = [UIButton buttonWithType:UIButtonTypeSystem];
  primaryButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.background.backgroundColor = [UIColor blueColor];
  buttonConfiguration.baseForegroundColor = [UIColor whiteColor];
  buttonConfiguration.background.cornerRadius = 10;
  buttonConfiguration.title = PrimaryButtonTitle(_purpose);
  primaryButton.configuration = buttonConfiguration;

  [primaryButton addTarget:self
                    action:@selector(primaryButtonTapped:)
          forControlEvents:UIControlEventTouchUpInside];

  [self.view addSubview:primaryButton];
  [NSLayoutConstraint activateConstraints:@[
    [primaryButton.centerYAnchor
        constraintEqualToAnchor:self.view.centerYAnchor],
    [primaryButton.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
  ]];
}

#pragma mark - Private

// Handles taps on the primary button.
- (void)primaryButtonTapped:(UIButton*)sender {
  ProceduralBlock primaryButtonAction = _primaryButtonAction;
  _primaryButtonAction = nil;

  primaryButtonAction();
}

@end
