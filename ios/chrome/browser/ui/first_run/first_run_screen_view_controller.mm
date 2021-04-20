// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/first_run/first_run_screen_view_controller.h"

#include "base/check.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kFirstRunTitleAccessibilityIdentifier =
    @"kFirstRunTitleAccessibilityIdentifier";
NSString* const kFirstRunSubtitleAccessibilityIdentifier =
    @"kFirstRunSubtitleAccessibilityIdentifier";
NSString* const kFirstRunPrimaryActionAccessibilityIdentifier =
    @"kFirstRunPrimaryActionAccessibilityIdentifier";
NSString* const kFirstRunSecondaryActionAccessibilityIdentifier =
    @"kFirstRunSecondaryActionAccessibilityIdentifier";
NSString* const kFirstRunTertiaryActionAccessibilityIdentifier =
    @"kFirstRunTertiaryActionAccessibilityIdentifier";

namespace {

constexpr CGFloat kDefaultMargin = 16;
constexpr CGFloat kActionsBottomMargin = 10;
constexpr CGFloat kTallBannerMultiplier = 0.35;
constexpr CGFloat kDefaultBannerMultiplier = 0.25;
constexpr CGFloat kSubtitleBottomMarginViewHeight = 0.05;

}  // namespace

@interface FirstRunScreenViewController ()

@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) UIImageView* imageView;
// UIView that wraps the scrollable content.
@property(nonatomic, strong) UIView* scrollContentView;
@property(nonatomic, strong) UILabel* titleLabel;
@property(nonatomic, strong) UILabel* subtitleLabel;
@property(nonatomic, strong) UIButton* primaryActionButton;
@property(nonatomic, strong) UIButton* secondaryActionButton;
@property(nonatomic, strong) UIButton* tertiaryActionButton;

@end

@implementation FirstRunScreenViewController

#pragma mark - Public

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];

  // Some of the margins are dynamic, based on the view height. Create an empty
  // view, for which the height can be constrained to the view height, that will
  // act as the dynamic margin.
  UIView* subtitleBottomMarginView = [[UIView alloc] init];
  subtitleBottomMarginView.translatesAutoresizingMaskIntoConstraints = NO;

  self.scrollContentView = [[UIView alloc] init];
  self.scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.scrollContentView addSubview:self.imageView];
  [self.scrollContentView addSubview:self.titleLabel];
  [self.scrollContentView addSubview:self.subtitleLabel];
  [self.scrollContentView addSubview:subtitleBottomMarginView];
  [self.scrollContentView addSubview:self.specificContentView];

  // Wrap everything except the action buttons in a scroll view, to support
  // dynamic types.
  [self.scrollView addSubview:self.scrollContentView];
  [self.view addSubview:self.scrollView];

  UIStackView* actionStackView = [[UIStackView alloc] init];
  actionStackView.alignment = UIStackViewAlignmentFill;
  actionStackView.axis = UILayoutConstraintAxisVertical;
  actionStackView.translatesAutoresizingMaskIntoConstraints = NO;
  if (self.tertiaryActionString) {
    [actionStackView addArrangedSubview:self.tertiaryActionButton];
  }
  [actionStackView addArrangedSubview:self.primaryActionButton];
  if (self.secondaryActionString) {
    [actionStackView addArrangedSubview:self.secondaryActionButton];
  }
  [self.view addSubview:actionStackView];

  CGFloat actionStackViewTopMargin = 0.0;
  if (!self.tertiaryActionString) {
    actionStackViewTopMargin = -kDefaultMargin;
  }

  CGFloat bannerMultiplier =
      self.isTallBanner ? kTallBannerMultiplier : kDefaultBannerMultiplier;

  CGFloat extraBottomMargin =
      (self.secondaryActionString || self.tertiaryActionString)
          ? 0
          : kActionsBottomMargin;

  [NSLayoutConstraint activateConstraints:@[
    // Scroll view constraints.
    [self.scrollView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.scrollView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [self.scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.scrollView.bottomAnchor
        constraintEqualToAnchor:actionStackView.topAnchor
                       constant:actionStackViewTopMargin],

    // Scroll content view constraints. Constrain its edges to the scroll view
    // edges with some margins on the sides, but also constrain its height to at
    // least the scroll view height, so that derived VCs can pin UI elements
    // just above the buttons.
    [self.scrollContentView.topAnchor
        constraintEqualToAnchor:self.scrollView.topAnchor],
    [self.scrollContentView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kDefaultMargin],
    [self.scrollContentView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kDefaultMargin],
    [self.scrollContentView.bottomAnchor
        constraintEqualToAnchor:self.scrollView.bottomAnchor],
    [self.scrollContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:self.scrollView.heightAnchor],

    // Banner image constraints. Scale the image vertically so its height takes
    // a certain % of the view height while maintaining its aspect ratio. Don't
    // constrain the width so that the image extends all the way to the edges of
    // the view, outside the scrollContentView.
    [self.imageView.topAnchor
        constraintEqualToAnchor:self.scrollContentView.topAnchor],
    [self.imageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.imageView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor
                                              multiplier:bannerMultiplier],

    // Labels contraints. Attach them to the top of the scroll content view, and
    // center them horizontally.
    [self.titleLabel.topAnchor
        constraintEqualToAnchor:self.imageView.bottomAnchor],
    [self.titleLabel.centerXAnchor
        constraintEqualToAnchor:self.scrollContentView.centerXAnchor],
    [self.titleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.scrollContentView.widthAnchor],
    [self.subtitleLabel.topAnchor
        constraintEqualToAnchor:self.titleLabel.bottomAnchor
                       constant:kDefaultMargin],
    [self.subtitleLabel.centerXAnchor
        constraintEqualToAnchor:self.scrollContentView.centerXAnchor],
    [self.subtitleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.scrollContentView.widthAnchor],

    // Constraints for the screen-specific content view. It should take the
    // remaining scroll view area, with some margins on the top and sides.
    [subtitleBottomMarginView.topAnchor
        constraintEqualToAnchor:self.subtitleLabel.bottomAnchor],
    [subtitleBottomMarginView.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor
                     multiplier:kSubtitleBottomMarginViewHeight],
    [self.specificContentView.topAnchor
        constraintEqualToAnchor:subtitleBottomMarginView.bottomAnchor],
    [self.specificContentView.leadingAnchor
        constraintEqualToAnchor:self.scrollContentView.leadingAnchor],
    [self.specificContentView.trailingAnchor
        constraintEqualToAnchor:self.scrollContentView.trailingAnchor],
    [self.specificContentView.bottomAnchor
        constraintEqualToAnchor:self.scrollContentView.bottomAnchor],

    // Action stack view constraints. Constrain the bottom of the action stack
    // view to both the bottom of the screen and the bottom of the safe area, to
    // give a nice result whether the device has a physical home button or not.
    [actionStackView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kDefaultMargin],
    [actionStackView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kDefaultMargin],
    [actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                 constant:-kActionsBottomMargin -
                                          extraBottomMargin],
    [actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor
                                 constant:-extraBottomMargin]
  ]];

  // Also constrain the bottom of the action stack view to the bottom of the
  // safe area, but with a lower priority, so that the action stack view is put
  // as close to the bottom as possible.
  NSLayoutConstraint* actionBottomConstraint = [actionStackView.bottomAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
  actionBottomConstraint.priority = UILayoutPriorityDefaultLow;
  actionBottomConstraint.active = YES;
}

- (void)setPrimaryActionString:(NSString*)text {
  _primaryActionString = text;
  if (_primaryActionButton) {
    [_primaryActionButton setTitle:_primaryActionString
                          forState:UIControlStateNormal];
  }
}

#pragma mark - Private

- (UIScrollView*)scrollView {
  if (!_scrollView) {
    _scrollView = [[UIScrollView alloc] init];
    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _scrollView;
}

- (UIImageView*)imageView {
  if (!_imageView) {
    _imageView = [[UIImageView alloc] initWithImage:self.bannerImage];
    _imageView.contentMode = UIViewContentModeScaleAspectFill;
    _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _imageView;
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 0;
    UIFontDescriptor* descriptor = [UIFontDescriptor
        preferredFontDescriptorWithTextStyle:UIFontTextStyleLargeTitle];
    UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                     weight:UIFontWeightBold];
    UIFontMetrics* fontMetrics =
        [UIFontMetrics metricsForTextStyle:UIFontTextStyleLargeTitle];
    _titleLabel.font = [fontMetrics scaledFontForFont:font];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.text = self.titleText;
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.accessibilityIdentifier = kFirstRunTitleAccessibilityIdentifier;
  }
  return _titleLabel;
}

- (UILabel*)subtitleLabel {
  if (!_subtitleLabel) {
    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
    _subtitleLabel.numberOfLines = 0;
    _subtitleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _subtitleLabel.text = self.subtitleText;
    _subtitleLabel.textAlignment = NSTextAlignmentCenter;
    _subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleLabel.adjustsFontForContentSizeCategory = YES;
    _subtitleLabel.accessibilityIdentifier =
        kFirstRunSubtitleAccessibilityIdentifier;
  }
  return _subtitleLabel;
}

- (UIView*)specificContentView {
  if (!_specificContentView) {
    _specificContentView = [[UIView alloc] init];
    _specificContentView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _specificContentView;
}

- (UIButton*)primaryActionButton {
  if (!_primaryActionButton) {
    _primaryActionButton = PrimaryActionButton(YES);
    [_primaryActionButton setTitle:self.primaryActionString
                          forState:UIControlStateNormal];
    _primaryActionButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    _primaryActionButton.accessibilityIdentifier =
        kFirstRunPrimaryActionAccessibilityIdentifier;
    [_primaryActionButton addTarget:self
                             action:@selector(didTapPrimaryActionButton)
                   forControlEvents:UIControlEventTouchUpInside];
  }
  return _primaryActionButton;
}

- (UIButton*)secondaryActionButton {
  if (!_secondaryActionButton) {
    DCHECK(self.secondaryActionString);
    _secondaryActionButton =
        [self createButtonWithText:self.secondaryActionString
            accessibilityIdentifier:
                kFirstRunSecondaryActionAccessibilityIdentifier];
    [_secondaryActionButton addTarget:self
                               action:@selector(didTapSecondaryActionButton)
                     forControlEvents:UIControlEventTouchUpInside];
  }

  return _secondaryActionButton;
}

- (UIButton*)tertiaryActionButton {
  if (!_tertiaryActionButton) {
    DCHECK(self.tertiaryActionString);
    _tertiaryActionButton = [self
           createButtonWithText:self.tertiaryActionString
        accessibilityIdentifier:kFirstRunTertiaryActionAccessibilityIdentifier];
    [_tertiaryActionButton addTarget:self
                              action:@selector(didTapTertiaryActionButton)
                    forControlEvents:UIControlEventTouchUpInside];
  }

  return _tertiaryActionButton;
}

- (UIButton*)createButtonWithText:(NSString*)buttonText
          accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitle:buttonText forState:UIControlStateNormal];
  button.contentEdgeInsets =
      UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  [button setBackgroundColor:[UIColor clearColor]];
  UIColor* titleColor = [UIColor colorNamed:kBlueColor];
  [button setTitleColor:titleColor forState:UIControlStateNormal];
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.accessibilityIdentifier = accessibilityIdentifier;

  if (@available(iOS 13.4, *)) {
    button.pointerInteractionEnabled = YES;
    button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();
  }

  return button;
}

- (void)didTapPrimaryActionButton {
  if ([self.delegate respondsToSelector:@selector(didTapPrimaryActionButton)]) {
    [self.delegate didTapPrimaryActionButton];
  }
}

- (void)didTapSecondaryActionButton {
  DCHECK(self.secondaryActionString);
  if ([self.delegate
          respondsToSelector:@selector(didTapSecondaryActionButton)]) {
    [self.delegate didTapSecondaryActionButton];
  }
}

- (void)didTapTertiaryActionButton {
  DCHECK(self.tertiaryActionString);
  if ([self.delegate
          respondsToSelector:@selector(didTapTertiaryActionButton)]) {
    [self.delegate didTapTertiaryActionButton];
  }
}

@end
