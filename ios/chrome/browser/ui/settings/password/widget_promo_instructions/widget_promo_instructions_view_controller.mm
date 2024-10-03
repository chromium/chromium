// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/elements/instruction_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/settings/password/widget_promo_instructions/widget_promo_instructions_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

@implementation WidgetPromoInstructionsViewController {
  // The constraint setting the image view's height to zero. Used to hide the
  // image when in lanscape mode.
  NSLayoutConstraint* _imageViewZeroHeightConstraint;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.view.accessibilityIdentifier =
      password_manager::kWidgetPromoInstructionsViewID;

  UIImageView* imageView = [self createImageView];
  ConfirmationAlertViewController* instructionsViewController =
      [self createInstructionsViewController];

  [self.view addSubview:imageView];
  [self addChildViewController:instructionsViewController];
  [self.view addSubview:instructionsViewController.view];
  [instructionsViewController didMoveToParentViewController:self];

  [NSLayoutConstraint activateConstraints:@[
    // `imageView` constraints.
    [imageView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [imageView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor],

    // `instructionsViewController` constraints.
    [instructionsViewController.view.topAnchor
        constraintEqualToAnchor:imageView.bottomAnchor
                       constant:32],
    [instructionsViewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [instructionsViewController.view.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [instructionsViewController.view.widthAnchor
        constraintEqualToAnchor:self.view.widthAnchor],

  ]];

  _imageViewZeroHeightConstraint =
      [imageView.heightAnchor constraintEqualToConstant:0];
  [self updateImageViewVisibility];

  if (@available(iOS 17, *)) {
    NSArray<UITrait>* traits =
        TraitCollectionSetForTraits(@[ UITraitVerticalSizeClass.self ]);
    [self registerForTraitChanges:traits
                       withAction:@selector(updateImageViewVisibility)];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (self.traitCollection.verticalSizeClass !=
      previousTraitCollection.verticalSizeClass) {
    [self updateImageViewVisibility];
  }
}
#endif

#pragma mark - Private

// Creates and configures the image view.
- (UIImageView*)createImageView {
  UIImageView* imageView = [[UIImageView alloc]
      initWithImage:[UIImage imageNamed:password_manager::
                                            kWidgetPromoInstructionsImageName]];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.contentMode = UIViewContentModeScaleAspectFit;
  imageView.accessibilityIdentifier =
      password_manager::kWidgetPromoInstructionsImageID;

  return imageView;
}

// Creates and configures the instructions view controller.
- (ConfirmationAlertViewController*)createInstructionsViewController {
  ConfirmationAlertViewController* instructionsViewController =
      [[ConfirmationAlertViewController alloc] init];

  instructionsViewController.titleString =
      l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_TITLE);
  instructionsViewController.titleTextStyle = UIFontTextStyleTitle2;
  instructionsViewController.subtitleString =
      l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_SUBTITLE);
  instructionsViewController.subtitleTextStyle = UIFontTextStyleBody;
  instructionsViewController.secondaryActionString =
      l10n_util::GetNSString(IDS_CLOSE);
  instructionsViewController.showDismissBarButton = NO;
  instructionsViewController.topAlignedLayout = YES;
  instructionsViewController.imageHasFixedSize = YES;
  instructionsViewController.actionHandler = self.actionHandler;

  InstructionView* instructionView = [self createInstructionView];
  instructionsViewController.underTitleView = instructionView;

  instructionsViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  instructionsViewController.view.accessibilityIdentifier =
      password_manager::kWidgetPromoInstructionsScrollableViewID;

  return instructionsViewController;
}

// Creates and configures the instruction view.
- (InstructionView*)createInstructionView {
  NSArray<NSString*>* steps = @[
    l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_STEP_1),
    l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_STEP_2),
    l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_STEP_3),
    l10n_util::GetNSString(IDS_IOS_WIDGET_PROMO_INSTRUCTIONS_STEP_4),
  ];

  InstructionView* instructionView =
      [[InstructionView alloc] initWithList:steps];
  instructionView.translatesAutoresizingMaskIntoConstraints = NO;

  return instructionView;
}

// Updates the `imageView` visibility depending on the device's orientation. The
// image should only be visible in portait mode.
- (void)updateImageViewVisibility {
  _imageViewZeroHeightConstraint.active = IsCompactHeight(self);
}

@end
