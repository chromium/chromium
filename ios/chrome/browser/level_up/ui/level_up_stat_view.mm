// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/level_up/ui/level_up_stat_view.h"

#import "ios/chrome/browser/level_up/model/task_types.h"
#import "ios/chrome/browser/shared/ui/animated_promo/animated_promo_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_api.h"
#import "ios/public/provider/chrome/browser/lottie/lottie_animation_configuration.h"
#import "ui/gfx/color_palette.h"

namespace {

// Spacing for layout margins and stacks.
const CGFloat kLayoutSpacing = 16.0;
// The size of the illustration container.
const CGFloat kIllustrationSize = 100.0;
// Spacing between the illustration and the text.
const CGFloat kIllustrationTextSpacing = 8.0;
// Spacing within the vertical text labels stack.
const CGFloat kTextStackSpacing = 8.0;

// The corner radius of the card.
const CGFloat kCardCornerRadius = 24.0;
// The opacity of the card shadow.
const CGFloat kCardShadowOpacity = 1.0;
// The blur radius of the card shadow.
const CGFloat kCardShadowRadius = 2.0;
// The vertical offset of the card shadow.
const CGFloat kCardShadowOffset = 1.0;
// The color alpha of the card shadow.
const CGFloat kCardShadowAlpha = 0.05;

}  // namespace

@implementation LevelUpStatView {
  // Label displaying the stat title/metric.
  UILabel* _titleLabel;
  // Label displaying the stat subtitle description.
  UILabel* _subtitleLabel;
  // The stack view containing the card contents.
  UIStackView* _cardStack;
  // The Lottie animation wrapper.
  id<LottieAnimation> _lottieAnimation;
  // The Lottie view.
  UIView* _lottieView;
  // The stat type represented by the card.
  LevelUpTaskStatType _statType;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.contentView.backgroundColor =
        [UIColor colorNamed:kPrimaryBackgroundColor];
    self.contentView.layer.cornerRadius = kCardCornerRadius;
    self.contentView.layer.masksToBounds = YES;
    self.translatesAutoresizingMaskIntoConstraints = NO;

    self.layer.shadowColor =
        [UIColor colorWithRed:0 green:0 blue:0 alpha:kCardShadowAlpha].CGColor;
    self.layer.shadowOpacity = kCardShadowOpacity;
    self.layer.shadowRadius = kCardShadowRadius;
    self.layer.shadowOffset = CGSizeMake(0, kCardShadowOffset);
    self.layer.masksToBounds = NO;

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    UIFontDescriptor* bodyDescriptor = [UIFontDescriptor
        preferredFontDescriptorWithTextStyle:UIFontTextStyleBody];
    _titleLabel.font = [UIFont systemFontOfSize:bodyDescriptor.pointSize
                                         weight:UIFontWeightSemibold];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.numberOfLines = 0;

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitleLabel.numberOfLines = 0;

    UIStackView* textStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel ]];
    textStack.translatesAutoresizingMaskIntoConstraints = NO;
    textStack.axis = UILayoutConstraintAxisVertical;
    textStack.spacing = kTextStackSpacing;

    _cardStack = [[UIStackView alloc] initWithArrangedSubviews:@[ textStack ]];
    _cardStack.translatesAutoresizingMaskIntoConstraints = NO;
    _cardStack.axis = UILayoutConstraintAxisHorizontal;
    _cardStack.spacing = kIllustrationTextSpacing;
    _cardStack.alignment = UIStackViewAlignmentCenter;

    [self.contentView addSubview:_cardStack];

    AddSameConstraintsWithInsets(
        _cardStack, self.contentView,
        NSDirectionalEdgeInsetsMake(kLayoutSpacing, kLayoutSpacing,
                                    kLayoutSpacing, kLayoutSpacing));

    [self registerForTraitChanges:@[ UITraitUserInterfaceStyle.class ]
                       withAction:@selector(configureAnimationColors)];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  self.layer.shadowPath =
      [UIBezierPath bezierPathWithRoundedRect:self.bounds
                                 cornerRadius:kCardCornerRadius]
          .CGPath;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  _titleLabel.text = nil;
  _subtitleLabel.text = nil;
  [_lottieView removeFromSuperview];
  _lottieView = nil;
  _lottieAnimation = nil;
}

- (void)setStatTitle:(NSString*)title
            subtitle:(NSString*)subtitle
     imageLottieName:(NSString*)imageLottieName
            statType:(LevelUpTaskStatType)statType {
  _titleLabel.text = title;
  _subtitleLabel.text = subtitle;
  _statType = statType;

  [_lottieView removeFromSuperview];
  _lottieView = nil;
  _lottieAnimation = nil;

  LottieAnimationConfiguration* config =
      [[LottieAnimationConfiguration alloc] init];
  config.animationName = imageLottieName;

  _lottieAnimation = ios::provider::GenerateLottieAnimation(config);
  [self configureAnimationColors];

  _lottieView = _lottieAnimation.animationView;
  _lottieView.translatesAutoresizingMaskIntoConstraints = NO;
  _lottieView.contentMode = UIViewContentModeScaleAspectFit;

  [_cardStack insertArrangedSubview:_lottieView atIndex:0];
  [NSLayoutConstraint activateConstraints:@[
    [_lottieView.widthAnchor constraintEqualToConstant:kIllustrationSize],
    [_lottieView.heightAnchor constraintEqualToConstant:kIllustrationSize],
  ]];
}

#pragma mark - Private

// Configures the Lottie animation with semantic colors for light and dark
// themes.
- (void)configureAnimationColors {
  if (!_lottieAnimation) {
    return;
  }

  switch (_statType) {
    case LevelUpTaskStatType::kTabsDecluttered:
      [self configureTabsDeclutteredAnimationColors];
      break;
    case LevelUpTaskStatType::kPasswordsAutofilled:
      [self configurePasswordsAutofilledAnimationColors];
      break;
    case LevelUpTaskStatType::kPasswordsVerified:
      [self configurePasswordsVerifiedAnimationColors];
      break;
    case LevelUpTaskStatType::kPhotoSearchesPerformed:
      [self configurePhotoSearchesPerformedAnimationColors];
      break;
  }
}

// Configures colors for the `kTabsDecluttered` animation.
- (void)configureTabsDeclutteredAnimationColors {
  ConfigureAnimationCustomColor(_lottieAnimation, @"yellow_800_color",
                                gfx::kGoogleYellow800, gfx::kGoogleYellow100);
  ConfigureAnimationSemanticColor(_lottieAnimation, kYellow500Color,
                                  kYellow500Color);
  ConfigureAnimationCustomColor(_lottieAnimation, @"yellow_100_color",
                                gfx::kGoogleYellow100, gfx::kGoogleYellow800);
  ConfigureAnimationSemanticColor(_lottieAnimation,
                                  kAimComposeboxButtonBackgroundColor,
                                  kAimComposeboxButtonBackgroundColor);
  ConfigureAnimationCustomColor(_lottieAnimation, @"blue_800_color",
                                gfx::kGoogleBlue800, gfx::kGoogleBlue100);
  ConfigureAnimationSemanticColor(_lottieAnimation, kBlue100Color,
                                  kBlue100Color);
  ConfigureAnimationSemanticColor(_lottieAnimation, kGreen800Color,
                                  kGreen800Color);
  ConfigureAnimationSemanticColor(_lottieAnimation, kGreen100Color,
                                  kGreen100Color);
  ConfigureAnimationSemanticColor(_lottieAnimation, kGrey100Color,
                                  kGrey100Color);
}

// Configures colors for the `kPasswordsAutofilled` animation.
- (void)configurePasswordsAutofilledAnimationColors {
  ConfigureAnimationSemanticColor(_lottieAnimation, kGrey300Color,
                                  kGrey300Color);
  ConfigureAnimationSemanticColor(_lottieAnimation, kYellow600Color,
                                  kYellow600Color);
  ConfigureAnimationSemanticColor(_lottieAnimation, kGrey700Color,
                                  kGrey700Color);
  ConfigureAnimationSemanticColor(_lottieAnimation, kBackgroundColor,
                                  kBackgroundColor);
  ConfigureAnimationSemanticColor(_lottieAnimation, kYellow500Color,
                                  kYellow500Color);
}

// Configures colors for the `kPasswordsVerified` animation.
- (void)configurePasswordsVerifiedAnimationColors {
  ConfigureAnimationSemanticColor(_lottieAnimation, kGreen500Color,
                                  kGreen500Color);
}

// Configures colors for the `kPhotoSearchesPerformed` animation.
- (void)configurePhotoSearchesPerformedAnimationColors {
  ConfigureAnimationSemanticColor(_lottieAnimation, kBackgroundColor,
                                  kBackgroundColor);
  ConfigureAnimationSemanticColor(_lottieAnimation, kPurple500Color,
                                  kPurple500Color);
}

@end
