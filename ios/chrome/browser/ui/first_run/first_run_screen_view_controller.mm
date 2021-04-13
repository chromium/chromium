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

}  // namespace

@interface FirstRunScreenViewController ()

// UIView that contains the title, subtitle and screen-specific content.
@property(nonatomic, strong) UIScrollView* scrollView;
@property(nonatomic, strong) UIImageView* imageView;
@property(nonatomic, strong) UIView* wrapperView;
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

  // TODO(crbug.com/1186762): Add screen-specific content view.
  self.wrapperView = [[UIView alloc] init];
  self.wrapperView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.wrapperView addSubview:self.titleLabel];
  [self.wrapperView addSubview:self.subtitleLabel];

  // Wrap everything except the action buttons in a scroll view, to support
  // dynamic types.
  [self.scrollView addSubview:self.imageView];
  [self.scrollView addSubview:self.wrapperView];
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

  CGFloat extraMargin =
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

    // Banner image constraints. Scale the image vertically so its height takes
    // a certain % of the view height while maintaining its aspect ratio.
    [self.imageView.topAnchor
        constraintEqualToAnchor:self.scrollView.topAnchor],
    [self.imageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
    [self.imageView.heightAnchor constraintEqualToAnchor:self.view.heightAnchor
                                              multiplier:bannerMultiplier],

    // Shared content wrapper view constraints.
    [self.wrapperView.topAnchor
        constraintEqualToAnchor:self.imageView.bottomAnchor],
    [self.wrapperView.leadingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor
                       constant:kDefaultMargin],
    [self.wrapperView.trailingAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor
                       constant:-kDefaultMargin],
    [self.wrapperView.bottomAnchor
        constraintEqualToAnchor:self.scrollView.bottomAnchor],
    // TODO(crbug.com/1186762): Anchor wrapperView.bottomAnchor to the bottom of
    // the screen-specific content view (when it's added) instead of the
    // subtitle.
    [self.wrapperView.bottomAnchor
        constraintEqualToAnchor:self.subtitleLabel.bottomAnchor],

    [self.titleLabel.topAnchor
        constraintEqualToAnchor:self.wrapperView.topAnchor],
    [self.titleLabel.centerXAnchor
        constraintEqualToAnchor:self.wrapperView.centerXAnchor],
    [self.titleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.wrapperView.widthAnchor],
    [self.subtitleLabel.topAnchor
        constraintEqualToAnchor:self.titleLabel.bottomAnchor
                       constant:kDefaultMargin],
    [self.subtitleLabel.centerXAnchor
        constraintEqualToAnchor:self.wrapperView.centerXAnchor],
    [self.subtitleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:self.wrapperView.widthAnchor],

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
                                 constant:-kActionsBottomMargin - extraMargin],
    [actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                              .bottomAnchor
                                 constant:-extraMargin]
  ]];

  // Also constrain the bottom of the action stack view to the bottom of the
  // safe area, but with a lower priority, so that the action stack view is put
  // as close to the bottom as possible.
  NSLayoutConstraint* actionBottomConstraint = [actionStackView.bottomAnchor
      constraintEqualToAnchor:self.view.safeAreaLayoutGuide.bottomAnchor];
  actionBottomConstraint.priority = UILayoutPriorityDefaultLow;
  actionBottomConstraint.active = YES;
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

- (UIButton*)primaryActionButton {
  if (!_primaryActionButton) {
    _primaryActionButton = PrimaryActionButton(YES);
    [_primaryActionButton setTitle:self.primaryActionString
                          forState:UIControlStateNormal];
    _primaryActionButton.accessibilityIdentifier =
        kFirstRunPrimaryActionAccessibilityIdentifier;
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
  }

  return _secondaryActionButton;
}

- (UIButton*)tertiaryActionButton {
  if (!_tertiaryActionButton) {
    DCHECK(self.tertiaryActionString);
    _tertiaryActionButton = [self
           createButtonWithText:self.tertiaryActionString
        accessibilityIdentifier:kFirstRunTertiaryActionAccessibilityIdentifier];
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
  button.accessibilityIdentifier = accessibilityIdentifier;

  if (@available(iOS 13.4, *)) {
    button.pointerInteractionEnabled = YES;
    button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();
  }

  return button;
}

// TODO(crbug.com/1186762): Add action handler logic.

@end
