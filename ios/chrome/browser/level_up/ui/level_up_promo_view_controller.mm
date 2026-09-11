// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_promo_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Lottie animation asset name.
NSString* const kLevelUpPromoAnimationName = @"level_up_promo_animation";
// Spacing between the banner image and the title text.
constexpr CGFloat kBannerTitleGap = 24;
// Spacing between the subtitle and the checklist.
constexpr CGFloat kSubtitleBottomMargin = 16;
// Spacing between the text box boundary and the scroll container view.
constexpr CGFloat kSubtitleHorizontalMargin = 16;
// Checkmark icon point size in the checklist.
constexpr CGFloat kCheckmarkSymbolPointSize = 24;
// Vertical spacing between each item in the checklist.
constexpr CGFloat kRowSpacing = 12;
// Horizontal spacing between the checkmark icon and text in the checklist.
constexpr CGFloat kIconTextSpacing = 19;
// Left and right spacing from the checklist boundary to the screen edge.
constexpr CGFloat kChecklistHorizontalMargin = 34;
}  // namespace

@implementation LevelUpPromoViewController {
  // Lottie animation view wrapper.
  id<LottieAnimation> _animationViewWrapper;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(didTapDismissButton)];
  self.navigationItem.rightBarButtonItem = dismissButton;
  self.shouldHideBanner = NO;
  self.shouldHideBannerImage = YES;
  self.shouldBannerFillTopSpace = NO;
  self.bannerSize = BannerImageSizeType::kStandard;
  self.headerImageType = PromoStyleImageType::kNone;

  self.titleText = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_TITLE);
  self.subtitleText = l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_SUBTITLE);
  self.disclaimerText =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_PRIVACY_DISCLAIMER);
  self.subtitleBottomMargin = kSubtitleBottomMargin;
  self.titleTopMarginWhenNoHeaderImage = kBannerTitleGap;
  self.titleHorizontalMargin = 0;

  self.configuration.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_PRIMARY_BUTTON);
  self.configuration.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_SECONDARY_BUTTON);

  UIView* checklistView = [self createChecklistView];
  [self.specificContentView addSubview:checklistView];

  [NSLayoutConstraint activateConstraints:@[
    [checklistView.topAnchor
        constraintEqualToAnchor:self.specificContentView.topAnchor],
    [checklistView.bottomAnchor
        constraintEqualToAnchor:self.specificContentView.bottomAnchor],
    [checklistView.centerXAnchor
        constraintEqualToAnchor:self.specificContentView.centerXAnchor],
    [checklistView.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.specificContentView
                                                 .leadingAnchor
                                    constant:kChecklistHorizontalMargin],
    [checklistView.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.specificContentView
                                              .trailingAnchor
                                 constant:-kChecklistHorizontalMargin],
  ]];

  [super viewDidLoad];
  [NSLayoutConstraint activateConstraints:@[
    [self.subtitleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.widthAnchor
                                 constant:-2 * kSubtitleHorizontalMargin],
  ]];

  UIView* animationView = [self createAnimationView];
  [self.bannerImageView addSubview:animationView];
  AddSameConstraints(animationView, self.bannerImageView);

  [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                     withAction:@selector(configureAnimationColors)];
  [self configureAnimationColors];
}

#pragma mark - PromoStyleViewController

- (UIFontTextStyle)titleLabelFontTextStyle {
  return UIFontTextStyleTitle1;
}

#pragma mark - Private

// Creates the animation view and sets `_animationViewWrapper`.
- (UIView*)createAnimationView {
  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = kLevelUpPromoAnimationName;
  _animationViewWrapper = ios::provider::GenerateLottieAnimation(config);
  UIView* animationView = _animationViewWrapper.animationView;
  animationView.translatesAutoresizingMaskIntoConstraints = NO;
  animationView.contentMode = UIViewContentModeScaleAspectFit;
  return animationView;
}

// TODO(crbug.com/559166203): Update color hex with existing skcolors
// Configures the animation with semantic and custom colors for light and dark
// modes.
- (void)configureAnimationColors {
  // Green palette.
  ConfigureAnimationSemanticColor(_animationViewWrapper, kGreen100Color,
                                  kGreen100Color);
  ConfigureAnimationCustomColor(_animationViewWrapper, @"A8DAB5",
                                UIColorFromRGB(0XA8DAB5),
                                UIColorFromRGB(0X5BB974));
  ConfigureAnimationSemanticColor(_animationViewWrapper, @"kGreen400Keypath",
                                  kGreen400Color);
  ConfigureAnimationSemanticColor(_animationViewWrapper, @"kGreen800Keypath",
                                  kGreen800Color);

  // Yellow / Orange palette.
  ConfigureAnimationCustomColor(_animationViewWrapper, @"0XEA8600",
                                UIColorFromRGB(0XEA8600),
                                UIColorFromRGB(0XFEF7E0));
  ConfigureAnimationCustomColor(_animationViewWrapper, @"0xFEEFC3",
                                UIColorFromRGB(0xFEEFC3),
                                UIColorFromRGB(0xFA8600));
  ConfigureAnimationCustomColor(_animationViewWrapper, @"0xFDD663",
                                UIColorFromRGB(0xFDD663),
                                UIColorFromRGB(0xEA8600));
  ConfigureAnimationCustomColor(_animationViewWrapper, @"0xFBBC04",
                                UIColorFromRGB(0xFBBC04),
                                UIColorFromRGB(0xFCC934));

  // Purple palette.
  ConfigureAnimationCustomColor(_animationViewWrapper, @"0x7627BB",
                                UIColorFromRGB(0x7627BB),
                                UIColorFromRGB(0xF3E8FD));
  ConfigureAnimationCustomColor(_animationViewWrapper, @"0xF0DDF0",
                                UIColorFromRGB(0xF0DDF0),
                                UIColorFromRGB(0x8430CE));
  ConfigureAnimationCustomColor(_animationViewWrapper, @"0xA142F4",
                                UIColorFromRGB(0xA142F4),
                                UIColorFromRGB(0xC58AF9));
  ConfigureAnimationCustomColor(_animationViewWrapper, @"0xD7AEFB",
                                UIColorFromRGB(0xD7AEFB),
                                UIColorFromRGB(0xA142F4));
}

// Creates a checklist view with blue checkmarks, promo content, and a bottom
// spacer to keep items pinned to the top.
- (UIView*)createChecklistView {
  UIStackView* verticalStack = [self createChecklistRowsStackView];
  [verticalStack addArrangedSubview:[self createSpacerView]];
  return verticalStack;
}

// Creates a vertical stack view containing all the checklist row stacks.
- (UIStackView*)createChecklistRowsStackView {
  UIStackView* verticalStack = [[UIStackView alloc] init];
  verticalStack.translatesAutoresizingMaskIntoConstraints = NO;
  verticalStack.axis = UILayoutConstraintAxisVertical;
  verticalStack.spacing = kRowSpacing;
  verticalStack.alignment = UIStackViewAlignmentLeading;

  UIImage* checkmarkImage = [self checkmarkImage];
  for (NSString* itemText in [self checklistItems]) {
    UIStackView* rowStack = [self createChecklistRowWithText:itemText
                                              checkmarkImage:checkmarkImage];
    [verticalStack addArrangedSubview:rowStack];

    [NSLayoutConstraint activateConstraints:@[
      [rowStack.widthAnchor
          constraintLessThanOrEqualToAnchor:verticalStack.widthAnchor],
    ]];
  }

  return verticalStack;
}

// Creates a single checklist row stack with a checkmark icon and text label.
- (UIStackView*)createChecklistRowWithText:(NSString*)itemText
                            checkmarkImage:(UIImage*)checkmarkImage {
  UIStackView* rowStack = [[UIStackView alloc] init];
  rowStack.translatesAutoresizingMaskIntoConstraints = NO;
  rowStack.axis = UILayoutConstraintAxisHorizontal;
  rowStack.spacing = kIconTextSpacing;
  rowStack.alignment = UIStackViewAlignmentCenter;

  UIImageView* iconView = [[UIImageView alloc] initWithImage:checkmarkImage];
  iconView.translatesAutoresizingMaskIntoConstraints = NO;
  [iconView setContentHuggingPriority:UILayoutPriorityRequired
                              forAxis:UILayoutConstraintAxisHorizontal];
  [iconView
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];

  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.text = itemText;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  label.numberOfLines = 0;
  label.lineBreakMode = NSLineBreakByWordWrapping;
  label.adjustsFontForContentSizeCategory = YES;
  [label
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];

  [rowStack addArrangedSubview:iconView];
  [rowStack addArrangedSubview:label];

  return rowStack;
}

// Returns a blue checkmark symbol for the checklist items.
- (UIImage*)checkmarkImage {
  UIImage* checkmark =
      SymbolWithPointSize(SymbolCheckmark, kCheckmarkSymbolPointSize);
  return [checkmark imageWithTintColor:[UIColor colorNamed:kBlueColor]
                         renderingMode:UIImageRenderingModeAlwaysOriginal];
}

// Returns the list of localized strings for the checklist.
- (NSArray<NSString*>*)checklistItems {
  return @[
    l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_CONTENT_1),
    l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_CONTENT_2),
    l10n_util::GetNSString(IDS_IOS_LEVEL_UP_PROMO_CONTENT_3),
  ];
}

// Creates a spacer view that expands vertically to keep preceding rows pinned
// to the top of the stack.
- (UIView*)createSpacerView {
  UIView* spacerView = [[UIView alloc] init];
  spacerView.translatesAutoresizingMaskIntoConstraints = NO;
  [spacerView setContentHuggingPriority:UILayoutPriorityFittingSizeLevel
                                forAxis:UILayoutConstraintAxisVertical];
  [spacerView
      setContentCompressionResistancePriority:UILayoutPriorityFittingSizeLevel
                                      forAxis:UILayoutConstraintAxisVertical];
  return spacerView;
}

// Dismisses the level up promo view.
- (void)didTapDismissButton {
  [self.delegate didTapDismissButton];
}

@end
