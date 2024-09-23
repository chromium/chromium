// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/inactive_tabs_button_cell.h"

#import "base/strings/string_number_conversions.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
constexpr CGFloat kHorizontalPadding = 16;
constexpr CGFloat kVerticalPadding = 10;
constexpr CGFloat kInterTextSpacing = 4;
constexpr CGFloat kDefaultPadding = 8;
constexpr CGFloat kCornerRadius = 10;
}  // namespace

@implementation InactiveTabsButtonCell {
  UILabel* _titleLabel;
  UILabel* _subtitleLabel;
  UILabel* _countLabel;
  UIView* _disclosureIndicator;

  NSArray<NSLayoutConstraint*>* _regularConstraints;
  NSArray<NSLayoutConstraint*>* _accessibilityConstraints;
}

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    self.backgroundColor =
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
    self.layer.cornerRadius = kCornerRadius;
    self.accessibilityIdentifier = kInactiveTabsButtonAccessibilityIdentifier;
    self.isAccessibilityElement = YES;
    self.accessibilityTraits = UIAccessibilityTraitButton;
    [self updateAccessibilityLabel];

    UIView* contentView = self.contentView;

    _titleLabel = [self createTitleLabel];
    [contentView addSubview:_titleLabel];

    _subtitleLabel = [self createSubtitleLabel];
    [contentView addSubview:_subtitleLabel];

    _countLabel = [self createCountLabel];
    [contentView addSubview:_countLabel];

    UIView* disclosureIndicator = [self createDisclosureIndicator];
    [contentView addSubview:disclosureIndicator];

    UILayoutGuide* textGuide = [[UILayoutGuide alloc] init];
    [contentView addLayoutGuide:textGuide];

    [NSLayoutConstraint activateConstraints:@[
      [disclosureIndicator.trailingAnchor
          constraintEqualToAnchor:contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [disclosureIndicator.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],

      [textGuide.leadingAnchor constraintEqualToAnchor:contentView.leadingAnchor
                                              constant:kHorizontalPadding],
      [textGuide.topAnchor constraintEqualToAnchor:contentView.topAnchor
                                          constant:kVerticalPadding],
      [textGuide.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor
                                             constant:-kVerticalPadding],

      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:textGuide.leadingAnchor],
      [_titleLabel.topAnchor constraintEqualToAnchor:textGuide.topAnchor],
      [_titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:textGuide.trailingAnchor],

      [_subtitleLabel.leadingAnchor
          constraintEqualToAnchor:textGuide.leadingAnchor],
      [_subtitleLabel.bottomAnchor
          constraintEqualToAnchor:textGuide.bottomAnchor],
      [_subtitleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:textGuide.trailingAnchor],
    ]];

    _regularConstraints = @[
      [_countLabel.leadingAnchor
          constraintEqualToAnchor:textGuide.trailingAnchor
                         constant:kDefaultPadding],
      [_countLabel.trailingAnchor
          constraintEqualToAnchor:disclosureIndicator.leadingAnchor
                         constant:-kDefaultPadding],
      [_countLabel.centerYAnchor
          constraintEqualToAnchor:contentView.centerYAnchor],

      [_subtitleLabel.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor
                                               constant:kInterTextSpacing],
    ];

    _accessibilityConstraints = @[
      [_countLabel.leadingAnchor
          constraintEqualToAnchor:textGuide.leadingAnchor],
      [_countLabel.topAnchor constraintEqualToAnchor:_titleLabel.bottomAnchor
                                            constant:kDefaultPadding],
      [_countLabel.bottomAnchor constraintEqualToAnchor:_subtitleLabel.topAnchor
                                               constant:-kDefaultPadding],

      [textGuide.trailingAnchor
          constraintEqualToAnchor:disclosureIndicator.leadingAnchor
                         constant:-kDefaultPadding],
    ];

    if (UIContentSizeCategoryIsAccessibilityCategory(
            self.traitCollection.preferredContentSizeCategory)) {
      [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
    } else {
      [NSLayoutConstraint activateConstraints:_regularConstraints];
    }

    if (@available(iOS 17, *)) {
      [self
          registerForTraitChanges:@[ UITraitPreferredContentSizeCategory.class ]
                       withTarget:self
                           action:@selector
                           (updateConstraintsForFontSizeChange)];
    }
  }
  return self;
}

#pragma mark - Accessor

- (void)setCount:(NSInteger)count {
  _count = count;
  NSString* countText =
      count > 99 ? @"99+" : [NSString stringWithFormat:@"%ld", count];
  _countLabel.text = countText;

  [self updateAccessibilityLabel];
}

- (void)setDaysThreshold:(NSInteger)daysThreshold {
  _daysThreshold = daysThreshold;
  _subtitleLabel.text =
      l10n_util::GetNSStringF(IDS_IOS_INACTIVE_TABS_BUTTON_SUBTITLE,
                              base::NumberToString16(daysThreshold));

  [self updateAccessibilityLabel];
}

#pragma mark - UICollectionViewCell

- (void)setHighlighted:(BOOL)highlighted {
  [super setHighlighted:highlighted];
  if (highlighted) {
    self.backgroundColor = [UIColor systemGray4Color];
  } else {
    self.backgroundColor =
        [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
  }
}

#pragma mark - UIView

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }
  // Update constraints for user's preferredContentSize.
  if (self.traitCollection.preferredContentSizeCategory !=
      previousTraitCollection.preferredContentSizeCategory) {
    [self updateConstraintsForFontSizeChange];
  }
}
#endif

#pragma mark - Private

// Updates the constraints to take into account the accessibility category.
- (void)updateConstraintsForFontSizeChange {
  if (UIContentSizeCategoryIsAccessibilityCategory(
          self.traitCollection.preferredContentSizeCategory)) {
    [NSLayoutConstraint deactivateConstraints:_regularConstraints];
    [NSLayoutConstraint activateConstraints:_accessibilityConstraints];
  } else {
    [NSLayoutConstraint deactivateConstraints:_accessibilityConstraints];
    [NSLayoutConstraint activateConstraints:_regularConstraints];
  }
}

// Returns a configured title label.
- (UILabel*)createTitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.adjustsFontForContentSizeCategory = YES;
  label.text = l10n_util::GetNSString(IDS_IOS_INACTIVE_TABS_BUTTON_TITLE);
  label.textColor = UIColor.whiteColor;

  return label;
}

// Returns a configured subtitle label.
- (UILabel*)createSubtitleLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  label.adjustsFontForContentSizeCategory = YES;
  label.text =
      l10n_util::GetNSStringF(IDS_IOS_INACTIVE_TABS_BUTTON_SUBTITLE,
                              base::NumberToString16(self.daysThreshold));
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

// Returns a configured count label.
- (UILabel*)createCountLabel {
  UILabel* label = [[UILabel alloc] init];
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.adjustsFontForContentSizeCategory = YES;
  label.textColor = [UIColor colorNamed:kTextSecondaryColor];

  return label;
}

// Returns a configured disclosure indicator.
- (UIView*)createDisclosureIndicator {
  UIImageSymbolConfiguration* conf = [UIImageSymbolConfiguration
      configurationWithTextStyle:UIFontTextStyleBody];
  UIImageSymbolConfiguration* boldConf = [UIImageSymbolConfiguration
      configurationWithWeight:UIImageSymbolWeightSemibold];
  conf = [conf configurationByApplyingConfiguration:boldConf];
  UIImage* disclosure =
      DefaultSymbolWithConfiguration(kChevronForwardSymbol, conf);
  UIImageView* disclosureIndicator =
      [[UIImageView alloc] initWithImage:disclosure];
  disclosureIndicator.tintColor = [UIColor colorNamed:kTextTertiaryColor];
  disclosureIndicator.translatesAutoresizingMaskIntoConstraints = NO;

  return disclosureIndicator;
}

// Updates the accessibility label of the cell.
- (void)updateAccessibilityLabel {
  self.accessibilityLabel = [NSString
      stringWithFormat:@"%@, %@, %ld",
                       l10n_util::GetNSString(
                           IDS_IOS_INACTIVE_TABS_BUTTON_TITLE),
                       l10n_util::GetNSStringF(
                           IDS_IOS_INACTIVE_TABS_BUTTON_SUBTITLE,
                           base::NumberToString16(self.daysThreshold)),
                       self.count];
}

@end
