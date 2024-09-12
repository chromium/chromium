// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"

#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"

namespace {

const CGFloat kButtonHeight = 50.0f;

const NSDirectionalEdgeInsets dialogInsets = {20.0f, 20.0f, 20.0f, 20.0f};

NSString* const kLensUserEducationLightMode = @"lens_usered_lightmode";

NSString* const kLensUserEducationDarkMode = @"lens_usered_darkmode";

}  // namespace

@implementation LensOverlayConsentViewController {
  id<LottieAnimation> _animationViewWrapper;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // TODO(crbug.com/354942727): use color from mocks.
  self.view.backgroundColor = [UIColor systemBackgroundColor];

  // TODO(crbug.com/354942727): use strings from mocks and localize them.
  __weak __typeof(self) weakSelf = self;
  UIButton* acceptButton =
      [self newButtonWithTitle:@"Accept [TEST]"
                 actionHandler:^(UIAction* action) {
                   [weakSelf.delegate consentViewController:weakSelf
                                 didFinishWithTermsAccepted:YES];
                 }];

  UIButton* denyButton =
      [self newButtonWithTitle:@"Deny [TEST]"
                 actionHandler:^(UIAction* action) {
                   [weakSelf.delegate consentViewController:weakSelf
                                 didFinishWithTermsAccepted:NO];
                 }];

  NSArray<UIButton*>* buttons = @[ acceptButton, denyButton ];

  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = @"Lens Overlay [todo:localize]";

  UILabel* bodyLabel = [[UILabel alloc] init];
  bodyLabel.text =
      @"Wanna use Lens Overlay? Please give your consent [todo:localize]";
  bodyLabel.numberOfLines = 0;

  _animationViewWrapper =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
          ? [self createAnimation:kLensUserEducationDarkMode]
          : [self createAnimation:kLensUserEducationLightMode];

  _animationViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  _animationViewWrapper.animationView.contentMode =
      UIViewContentModeScaleAspectFit;

  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    titleLabel, _animationViewWrapper.animationView, bodyLabel, acceptButton,
    denyButton
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.alignment = UIStackViewAlignmentFill;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  verticalStack.spacing = 20.0f;

  [self.view addSubview:verticalStack];

  // Setup constraints

  for (UIButton* button in buttons) {
    [NSLayoutConstraint activateConstraints:@[
      [button.heightAnchor constraintGreaterThanOrEqualToConstant:kButtonHeight]
    ]];
  }

  AddSameConstraintsWithInsets(verticalStack, self.view, dialogInsets);
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_animationViewWrapper play];
}

- (UIButton*)newButtonWithTitle:(NSString*)title
                  actionHandler:(UIActionHandler)handler {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration filledButtonConfiguration];
  buttonConfiguration.title = title;
  UIButton* button =
      [UIButton buttonWithConfiguration:buttonConfiguration
                          primaryAction:[UIAction actionWithHandler:handler]];

  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;

  return button;
}

#pragma mark - Private

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = -1;  // Always loop.
  return ios::provider::GenerateLottieAnimation(config);
}

@end
