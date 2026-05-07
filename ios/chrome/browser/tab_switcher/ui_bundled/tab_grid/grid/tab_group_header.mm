// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/tab_group_header.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_group_edition_delegate.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/tab_groups/tab_groups_constants.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
constexpr CGFloat kDotTitleSeparationMargin = 8;
constexpr CGFloat kColoredDotSize = 20;
constexpr CGFloat kTitleHorizontalInset = 12;
constexpr CGFloat kTitleVerticalInset = 8;
constexpr CGFloat kTitleBackgroundCornerRadius = 16;
constexpr CGFloat kTitleBackgroundAlpha = 0.2;

// Returns the horizontal inset constraint for the title and dot views.
CGFloat GetHorizontalInsetForConstraints() {
  if (IsOpenEditGroupViewByTappingTitleEnabled()) {
    return kTitleHorizontalInset;
  }
  return 0;
}

// Returns the vertical inset constraint for the title view.
CGFloat GetVerticalInsetForConstraints() {
  if (IsOpenEditGroupViewByTappingTitleEnabled()) {
    return kTitleVerticalInset;
  }
  return 0;
}

}  // namespace

@implementation TabGroupHeader {
  // Title label.
  UILabel* _titleView;
  // Dot view.
  UIView* _coloredDotView;
  // The button container for the whole title.
  UIButton* _titleButton;
  // Constraints for regular width.
  NSArray<NSLayoutConstraint*>* _regularWidthConstraints;
  // Constraints for compact width.
  NSArray<NSLayoutConstraint*>* _compactWidthConstraints;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleView = [self titleView];
    _coloredDotView = [self coloredDotView];

    _titleButton = [self titleButton];
    _titleButton.translatesAutoresizingMaskIntoConstraints = NO;
    [self addSubview:_titleButton];

    [_titleButton addSubview:_coloredDotView];
    [_titleButton addSubview:_titleView];

    _regularWidthConstraints = @[
      [_titleButton.centerXAnchor constraintEqualToAnchor:self.centerXAnchor],
      [_titleButton.widthAnchor
          constraintLessThanOrEqualToAnchor:self.widthAnchor],
    ];

    _compactWidthConstraints = @[
      [_titleButton.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
      [_titleButton.trailingAnchor
          constraintLessThanOrEqualToAnchor:self.trailingAnchor],
    ];

    [NSLayoutConstraint activateConstraints:@[
      [_coloredDotView.leadingAnchor
          constraintEqualToAnchor:_titleButton.leadingAnchor
                         constant:GetHorizontalInsetForConstraints()],
      [_coloredDotView.centerYAnchor
          constraintEqualToAnchor:_titleView.centerYAnchor],

      [_titleView.leadingAnchor
          constraintEqualToAnchor:_coloredDotView.trailingAnchor
                         constant:kDotTitleSeparationMargin],

      [_titleView.trailingAnchor
          constraintEqualToAnchor:_titleButton.trailingAnchor
                         constant:-GetHorizontalInsetForConstraints()],
      [_titleView.topAnchor
          constraintEqualToAnchor:_titleButton.topAnchor
                         constant:GetVerticalInsetForConstraints()],
      [_titleView.bottomAnchor
          constraintEqualToAnchor:_titleButton.bottomAnchor
                         constant:-GetVerticalInsetForConstraints()],

      [_titleButton.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_titleButton.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];

    if (self.traitCollection.horizontalSizeClass ==
        UIUserInterfaceSizeClassRegular) {
      [NSLayoutConstraint activateConstraints:_regularWidthConstraints];
    } else {
      [NSLayoutConstraint activateConstraints:_compactWidthConstraints];
    }

    [self registerForTraitChanges:@[ UITraitHorizontalSizeClass.class ]
                       withAction:@selector(horizontalSizeClassDidChange)];
  }
  return self;
}

- (void)setTitle:(NSString*)title {
  if ([_title isEqual:title]) {
    return;
  }
  _title = title;
  UIFontDescriptor* boldDescriptor = [[UIFontDescriptor
      preferredFontDescriptorWithTextStyle:UIFontTextStyleLargeTitle]
      fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
  NSMutableAttributedString* boldTitle =
      [[NSMutableAttributedString alloc] initWithString:title];

  [boldTitle addAttribute:NSFontAttributeName
                    value:[UIFont fontWithDescriptor:boldDescriptor size:0.0]
                    range:NSMakeRange(0, title.length)];
  _titleView.attributedText = boldTitle;
  if (IsOpenEditGroupViewByTappingTitleEnabled()) {
    _titleButton.accessibilityLabel = title;
  }
}

- (void)setColor:(UIColor*)color {
  if ([_color isEqual:color]) {
    return;
  }
  _color = color;
  _coloredDotView.backgroundColor = color;

  if (!IsOpenEditGroupViewByTappingTitleEnabled()) {
    return;
  }

  UIButtonConfiguration* containerConfiguration = _titleButton.configuration;
  containerConfiguration.background.backgroundColor =
      [color colorWithAlphaComponent:kTitleBackgroundAlpha];
  _titleButton.configuration = containerConfiguration;
}

#pragma mark - Private

// Returns the group color dot view.
- (UIView*)coloredDotView {
  UIView* dotView = [[UIView alloc] initWithFrame:CGRectZero];
  dotView.translatesAutoresizingMaskIntoConstraints = NO;
  dotView.layer.cornerRadius = kColoredDotSize / 2;
  dotView.userInteractionEnabled = NO;

  [NSLayoutConstraint activateConstraints:@[
    [dotView.heightAnchor constraintEqualToConstant:kColoredDotSize],
    [dotView.widthAnchor constraintEqualToConstant:kColoredDotSize],
  ]];

  return dotView;
}

// Returns the title label view.
- (UILabel*)titleView {
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.textColor = UIColor.whiteColor;
  titleLabel.numberOfLines = 1;
  titleLabel.adjustsFontForContentSizeCategory = YES;
  titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  titleLabel.userInteractionEnabled = NO;
  return titleLabel;
}

// Called when the horizontal size class have changed.
- (void)horizontalSizeClassDidChange {
  if (self.traitCollection.horizontalSizeClass ==
      UIUserInterfaceSizeClassRegular) {
    [NSLayoutConstraint deactivateConstraints:_compactWidthConstraints];
    [NSLayoutConstraint activateConstraints:_regularWidthConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_regularWidthConstraints];
    [NSLayoutConstraint activateConstraints:_compactWidthConstraints];
  }
}

// Configures the title button.
- (UIButton*)titleButton {
  if (!IsOpenEditGroupViewByTappingTitleEnabled()) {
    return [[UIButton alloc] init];
  }

  UIBackgroundConfiguration* backgroundConfiguration =
      [UIBackgroundConfiguration clearConfiguration];
  // This visualEffect should stay in sync with the one used for the
  // buttons of the `topToolBar` in `TabGroupViewController`.
  backgroundConfiguration.visualEffect = [UIBlurEffect
      effectWithStyle:UIBlurEffectStyleSystemUltraThinMaterialDark];
  backgroundConfiguration.cornerRadius = kTitleBackgroundCornerRadius;

  UIButtonConfiguration* buttonConfiguration =
      [UIButtonConfiguration plainButtonConfiguration];
  buttonConfiguration.background = backgroundConfiguration;

  UIButton* titleButton = [UIButton buttonWithConfiguration:buttonConfiguration
                                              primaryAction:nil];

  titleButton.accessibilityIdentifier =
      kTabGroupTitleButtonToEditGroupIdentifier;
  titleButton.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TAB_GROUP_EDITION_ACCESSIBILITY_HINT);
  titleButton.accessibilityTraits |= UIAccessibilityTraitHeader;

  [titleButton addTarget:self
                  action:@selector(displayEditionMenu)
        forControlEvents:UIControlEventTouchUpInside];

  return titleButton;
}

// Shows the edition menu for the group.
- (void)displayEditionMenu {
  if ([self.tabGroupHeaderDelegate
          respondsToSelector:@selector(tabGroupHeaderDidTapTitle:)]) {
    [self.tabGroupHeaderDelegate tabGroupHeaderDidTapTitle:self];
  }
}

@end
