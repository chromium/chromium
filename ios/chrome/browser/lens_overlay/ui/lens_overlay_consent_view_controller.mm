// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"

#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

const NSDirectionalEdgeInsets dialogInsets = {26.0f, 20.0f, 44.0f, 20.0f};

NSString* const kLensUserEducationLightMode = @"lens_usered_lightmode";

NSString* const kLensUserEducationDarkMode = @"lens_usered_darkmode";

}  // namespace

@implementation LensOverlayConsentViewController {
  id<LottieAnimation> _animationViewWrapper;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  __weak __typeof(self) weakSelf = self;

  // Accept/deny terms buttons
  UIButton* acceptButton =
      [self acceptTermsButtonWithActionHandler:^(UIAction* action) {
        [weakSelf.delegate consentViewController:weakSelf
                      didFinishWithTermsAccepted:YES];
      }];
  UIButton* denyButton =
      [self denyTermsButtonWithActionHandler:^(UIAction* action) {
        [weakSelf.delegate consentViewController:weakSelf
                      didFinishWithTermsAccepted:NO];
      }];
  UIStackView* termsButtonsVerticalStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ acceptButton, denyButton ]];
  termsButtonsVerticalStack.axis = UILayoutConstraintAxisVertical;
  termsButtonsVerticalStack.alignment = UIStackViewAlignmentFill;
  termsButtonsVerticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  termsButtonsVerticalStack.spacing = 8;

  // Title/description labels.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.text = l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_CONSENT_TITLE);
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.font = [UIFont systemFontOfSize:22 weight:UIFontWeightBold];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  titleLabel.numberOfLines = 0;

  UILabel* bodyLabel = [[UILabel alloc] init];
  bodyLabel.text =
      l10n_util::GetNSString(IDS_IOS_LENS_OVERLAY_CONSENT_DESCRIPTION);
  bodyLabel.font = [UIFont systemFontOfSize:17 weight:UIFontWeightRegular];
  bodyLabel.adjustsFontForContentSizeCategory = YES;
  bodyLabel.textAlignment = NSTextAlignmentCenter;
  bodyLabel.numberOfLines = 0;
  bodyLabel.textColor =
      [UIColor colorNamed:kLensOverlayConsentDialogDescriptionColor];

  UIStackView* labelsVerticalStack =
      [[UIStackView alloc] initWithArrangedSubviews:@[ titleLabel, bodyLabel ]];
  labelsVerticalStack.axis = UILayoutConstraintAxisVertical;
  labelsVerticalStack.alignment = UIStackViewAlignmentFill;
  labelsVerticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  labelsVerticalStack.spacing = 8;

  // Lottie animation.
  _animationViewWrapper =
      self.traitCollection.userInterfaceStyle == UIUserInterfaceStyleDark
          ? [self createAnimation:kLensUserEducationDarkMode]
          : [self createAnimation:kLensUserEducationLightMode];

  _animationViewWrapper.animationView
      .translatesAutoresizingMaskIntoConstraints = NO;
  _animationViewWrapper.animationView.contentMode =
      UIViewContentModeScaleAspectFit;

  // The consent dialog vertical stack.
  UIStackView* verticalStack = [[UIStackView alloc] initWithArrangedSubviews:@[
    _animationViewWrapper.animationView, labelsVerticalStack, acceptButton,
    denyButton
  ]];
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.alignment = UIStackViewAlignmentFill;
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  verticalStack.spacing = 20.0f;

  [self.view addSubview:verticalStack];

  AddSameConstraintsWithInsets(verticalStack, self.view, dialogInsets);
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [_animationViewWrapper play];
}

- (CGSize)preferredContentSize {
  CGFloat fittingWidth = self.view.safeAreaLayoutGuide.layoutFrame.size.width;
  CGSize fittingSize =
      CGSizeMake(fittingWidth, UILayoutFittingCompressedSize.height);
  CGFloat height = [self.view systemLayoutSizeFittingSize:fittingSize].height;

  return CGSizeMake(fittingWidth, height);
}

#pragma mark - Private

- (UIButton*)acceptTermsButtonWithActionHandler:(UIActionHandler)handler {
  UIButtonConfiguration* buttonConfiguration =
      PrimaryActionButton(/*pointer_interaction_enabled=*/YES).configuration;
  buttonConfiguration.title = l10n_util::GetNSString(
      IDS_IOS_LENS_OVERLAY_CONSENT_ACCEPT_TERMS_BUTTON_TITLE);
  UIButton* button =
      [UIButton buttonWithConfiguration:buttonConfiguration
                          primaryAction:[UIAction actionWithHandler:handler]];

  button.layer.cornerRadius = 15;
  button.layer.masksToBounds = YES;
  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.pointerInteractionEnabled = YES;

  return button;
}

- (UIButton*)denyTermsButtonWithActionHandler:(UIActionHandler)handler {
  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.title = l10n_util::GetNSString(
      IDS_IOS_LENS_OVERLAY_CONSENT_DENY_TERMS_BUTTON_TITLE);

  UIButton* button =
      [UIButton buttonWithConfiguration:buttonConfiguration
                          primaryAction:[UIAction actionWithHandler:handler]];
  button.contentHorizontalAlignment = UIControlContentHorizontalAlignmentCenter;
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.pointerInteractionEnabled = YES;

  return button;
}

// Creates and returns the LottieAnimation view for the `animationAssetName`.
- (id<LottieAnimation>)createAnimation:(NSString*)animationAssetName {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = animationAssetName;
  config.loopAnimationCount = -1;  // Always loop.
  return ios::provider::GenerateLottieAnimation(config);
}

@end
