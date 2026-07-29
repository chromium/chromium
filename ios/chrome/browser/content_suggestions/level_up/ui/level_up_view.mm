// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/level_up/ui/level_up_view.h"

#import "ios/chrome/browser/content_suggestions/level_up/ui/level_up_config.h"
#import "ios/chrome/browser/content_suggestions/magic_stack/public/magic_stack_constants.h"
#import "ios/chrome/browser/level_up/ui/level_up_progress_bar.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_updating.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// Constants for the icon container view.
constexpr CGFloat kIconContainerSize = 56.0;
constexpr CGFloat kIconContainerCornerRadius = 12.0;

// Constants for the inner icon view.
constexpr CGFloat kInnerIconSize = 30.0;
constexpr CGFloat kInnerIconCornerRadius = 7.0;

// Spacing constants.
constexpr CGFloat kTitleDescriptionSpacing = 2.0;
// Spacing between the description label and the progress bar.
constexpr CGFloat kDescriptionProgressBarSpacing = 8.0;
constexpr CGFloat kContentStackSpacing = 14.0;

// Arrow icon constants.
constexpr CGFloat kArrowIconPointSize = 16.0;

}  // namespace

@interface LevelUpView () <NewTabPageColorUpdating>
@end

@implementation LevelUpView {
  LevelUpConfig* _config;
  UIView* _iconContainerView;
}

- (instancetype)initWithConfig:(LevelUpConfig*)config {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _config = config;

    [self registerForTraitChanges:@[ NewTabPageTrait.class ]
                       withAction:@selector(applyBackgroundColors)];
    [self createSubviews];
    [self applyBackgroundColors];
  }
  return self;
}

#pragma mark - NewTabPageColorUpdating

- (void)applyBackgroundColors {
  NewTabPageColorPalette* colorPalette =
      [self.traitCollection objectForNewTabPageTrait];
  _iconContainerView.backgroundColor =
      colorPalette.tertiaryColor ?: [UIColor colorNamed:kGrey100Color];
}

#pragma mark - Private

// Creates and configures the child views for the Level Up Magic Stack card.
- (void)createSubviews {
  self.translatesAutoresizingMaskIntoConstraints = NO;

  _iconContainerView = [self createIconContainerView];

  UILabel* titleLabel = [self createTitleLabel];
  UILabel* descriptionLabel = [self createDescriptionLabel];
  UIStackView* textStack =
      [self createTextStackViewWithTitleLabel:titleLabel
                             descriptionLabel:descriptionLabel];

  LevelUpProgressBar* progressBar = [self createProgressBar];
  [textStack addArrangedSubview:progressBar];
  [textStack setCustomSpacing:kDescriptionProgressBarSpacing
                    afterView:descriptionLabel];

  UIStackView* contentStack =
      [self createContentStackViewWithIconView:_iconContainerView
                                 textStackView:textStack];

  [self addSubview:contentStack];
  AddSameConstraints(contentStack, self);
}

// Creates the text stack view.
- (UIStackView*)createTextStackViewWithTitleLabel:(UILabel*)titleLabel
                                 descriptionLabel:(UILabel*)descriptionLabel {
  UIStackView* textStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ titleLabel, descriptionLabel ]];
  textStack.axis = UILayoutConstraintAxisVertical;
  textStack.translatesAutoresizingMaskIntoConstraints = NO;
  textStack.spacing = kTitleDescriptionSpacing;
  [textStack setContentHuggingPriority:UILayoutPriorityDefaultLow
                               forAxis:UILayoutConstraintAxisHorizontal];
  return textStack;
}

// Creates the progress bar.
- (LevelUpProgressBar*)createProgressBar {
  LevelUpProgressBar* progressBar = [[LevelUpProgressBar alloc] init];
  [progressBar setCompleted:_config.progressCompleted
                      total:_config.progressTotal];
  return progressBar;
}

// Creates the content stack view.
- (UIStackView*)createContentStackViewWithIconView:(UIView*)iconView
                                     textStackView:(UIView*)textStackView {
  UIStackView* contentStack = [[UIStackView alloc]
      initWithArrangedSubviews:@[ iconView, textStackView ]];
  contentStack.translatesAutoresizingMaskIntoConstraints = NO;
  contentStack.axis = UILayoutConstraintAxisHorizontal;
  contentStack.alignment = UIStackViewAlignmentCenter;
  contentStack.spacing = kContentStackSpacing;
  return contentStack;
}

// Creates the icon container and the inner purple arrow icon.
- (UIView*)createIconContainerView {
  UIView* containerView = [[UIView alloc] init];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  containerView.layer.cornerRadius = kIconContainerCornerRadius;
  containerView.layer.masksToBounds = YES;

  [NSLayoutConstraint activateConstraints:@[
    [containerView.widthAnchor constraintEqualToConstant:kIconContainerSize],
    [containerView.heightAnchor
        constraintEqualToAnchor:containerView.widthAnchor],
  ]];

  // Create the inner purple icon view.
  UIView* innerIconView = [[UIView alloc] init];
  innerIconView.translatesAutoresizingMaskIntoConstraints = NO;
  innerIconView.layer.cornerRadius = kInnerIconCornerRadius;
  innerIconView.layer.masksToBounds = YES;
  innerIconView.backgroundColor = [UIColor colorNamed:kPurple500Color];

  [NSLayoutConstraint activateConstraints:@[
    [innerIconView.widthAnchor constraintEqualToConstant:kInnerIconSize],
    [innerIconView.heightAnchor
        constraintEqualToAnchor:innerIconView.widthAnchor],
  ]];

  [containerView addSubview:innerIconView];
  AddSameCenterConstraints(innerIconView, containerView);

  // Create the white arrow icon inside the purple view.
  UIImageSymbolConfiguration* symbolConfig = [UIImageSymbolConfiguration
      configurationWithPointSize:kArrowIconPointSize
                          weight:UIImageSymbolWeightBold];
  UIImage* arrowImage =
      SymbolWithConfiguration(SymbolArrowshapeUp, symbolConfig);
  UIImageView* arrowImageView = [[UIImageView alloc] initWithImage:arrowImage];
  arrowImageView.translatesAutoresizingMaskIntoConstraints = NO;
  arrowImageView.tintColor = [UIColor whiteColor];
  arrowImageView.contentMode = UIViewContentModeScaleAspectFit;

  [innerIconView addSubview:arrowImageView];
  AddSameCenterConstraints(arrowImageView, innerIconView);

  return containerView;
}

// Creates the title label.
- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.text = _config.titleText;
  titleLabel.numberOfLines = 0;
  titleLabel.lineBreakMode = NSLineBreakByWordWrapping;
  titleLabel.font =
      PreferredFontForTextStyle(UIFontTextStyleFootnote, UIFontWeightSemibold,
                                kMaxTextSizeForStyleFootnote);
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.adjustsFontForContentSizeCategory = YES;
  return titleLabel;
}

// Creates the description label.
- (UILabel*)createDescriptionLabel {
  UILabel* descriptionLabel = [[UILabel alloc] init];
  descriptionLabel.translatesAutoresizingMaskIntoConstraints = NO;
  descriptionLabel.text = _config.descriptionText;
  descriptionLabel.numberOfLines = 2;
  descriptionLabel.lineBreakMode = NSLineBreakByTruncatingTail;
  descriptionLabel.font = PreferredFontForTextStyle(
      UIFontTextStyleFootnote, std::nullopt, kMaxTextSizeForStyleFootnote);
  descriptionLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  descriptionLabel.adjustsFontForContentSizeCategory = YES;
  return descriptionLabel;
}

@end
