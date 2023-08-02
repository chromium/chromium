// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/ios/app/first_launch_view_controller.h"

#import <MaterialComponents/MaterialButtons.h>

#include "remoting/base/string_resources.h"
#import "remoting/ios/app/remoting_theme.h"
#include "ui/base/l10n/l10n_util.h"

static const float kLogoSizeMultiplier = 0.381966f;
static const float kLogoYOffset = -10.f;
static const float kButtonHeight = 80.f;

@interface FirstLaunchViewController () {
  NSArray<NSLayoutConstraint*>* _compactWidthConstraints;
  NSArray<NSLayoutConstraint*>* _compactHeightConstraints;
}
@end

@implementation FirstLaunchViewController

@synthesize delegate = _delegate;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIImageView* imageView = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:@"launchscreen_app_logo"]];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:imageView];

  MDCFlatButton* signInButton = [[MDCFlatButton alloc] init];
  [signInButton setTitle:l10n_util::GetNSString(IDS_SIGN_IN_BUTTON)
                forState:UIControlStateNormal];
  [signInButton sizeToFit];
  [signInButton addTarget:self
                   action:@selector(didTapSignIn:)
         forControlEvents:UIControlEventTouchUpInside];
  [signInButton setTitleColor:RemotingTheme.flatButtonTextColor
                     forState:UIControlStateNormal];
  signInButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:signInButton];

  self.view.backgroundColor = RemotingTheme.firstLaunchViewBackgroundColor;

  [self initConstraints:imageView button:signInButton];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [self refreshTraitCollection];
}

#pragma mark - Private

- (void)didTapSignIn:(id)button {
  [_delegate presentSignInFlow];
}

- (void)initConstraints:(UIImageView*)imageView button:(UIButton*)signInButton {
  // This matches the constraints in LaunchScreen.storyboard.
  [imageView setContentHuggingPriority:UILayoutPriorityRequired
                               forAxis:UILayoutConstraintAxisVertical];
  [imageView setContentHuggingPriority:UILayoutPriorityRequired
                               forAxis:UILayoutConstraintAxisHorizontal];
  [imageView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisVertical];
  [imageView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultLow
                                      forAxis:UILayoutConstraintAxisHorizontal];

  _compactWidthConstraints =
      @[ [imageView.widthAnchor constraintEqualToAnchor:self.view.widthAnchor
                                             multiplier:kLogoSizeMultiplier] ];
  _compactWidthConstraints[0].priority = UILayoutPriorityDefaultHigh;

  _compactHeightConstraints =
      @[ [imageView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor
                                              multiplier:kLogoSizeMultiplier] ];
  _compactHeightConstraints[0].priority = UILayoutPriorityDefaultHigh;

  [NSLayoutConstraint activateConstraints:@[
    [imageView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],
    [imageView.centerYAnchor constraintEqualToAnchor:self.view.centerYAnchor
                                            constant:kLogoYOffset],
    [imageView.widthAnchor constraintEqualToAnchor:imageView.heightAnchor],

    [signInButton.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [signInButton.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [signInButton.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [signInButton.heightAnchor constraintEqualToConstant:kButtonHeight],
  ]];

  [self refreshTraitCollection];
}

- (void)refreshTraitCollection {
  if (self.traitCollection.verticalSizeClass ==
      UIUserInterfaceSizeClassCompact) {
    [NSLayoutConstraint deactivateConstraints:_compactWidthConstraints];
    [NSLayoutConstraint activateConstraints:_compactHeightConstraints];
  } else if (self.traitCollection.horizontalSizeClass ==
                 UIUserInterfaceSizeClassCompact &&
             self.traitCollection.verticalSizeClass ==
                 UIUserInterfaceSizeClassRegular) {
    [NSLayoutConstraint deactivateConstraints:_compactHeightConstraints];
    [NSLayoutConstraint activateConstraints:_compactWidthConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_compactWidthConstraints];
    [NSLayoutConstraint deactivateConstraints:_compactHeightConstraints];
  }
}

@end
