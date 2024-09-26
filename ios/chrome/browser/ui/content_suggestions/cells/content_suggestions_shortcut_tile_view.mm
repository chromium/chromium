// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_cells_constants.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"

namespace {

const CGFloat kCountWidth = 20;
const CGFloat kCountBorderWidth = 24;

}  // namespace

@implementation ContentSuggestionsShortcutTileView
@synthesize countLabel = _countLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame
                     tileType:ContentSuggestionsTileType::kShortcuts];
  if (self) {
    _iconView = [[UIImageView alloc] initWithFrame:self.bounds];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.imageContainerView addSubview:_iconView];
    AddSameConstraints(self.imageContainerView, _iconView);
    [NSLayoutConstraint activateConstraints:@[
      [_iconView.widthAnchor
          constraintEqualToConstant:kMagicStackImageContainerWidth],
      [_iconView.heightAnchor constraintEqualToAnchor:_iconView.widthAnchor],
    ]];

    self.imageBackgroundView.tintColor = [UIColor colorNamed:kBlueHaloColor];
    if (@available(iOS 17, *)) {
      [self registerViewForTraitChanges];
    }
  }
  return self;
}

- (instancetype)initWithConfiguration:
    (ContentSuggestionsMostVisitedActionItem*)config {
  self = [self initWithFrame:CGRectZero];
  if (self) {
    self.accessibilityCustomActions = nil;
    self.isAccessibilityElement = YES;
    _iconView.contentMode = UIViewContentModeCenter;

    [self updateConfiguration:config];

    if (@available(iOS 17, *)) {
      [self registerViewForTraitChanges];
    }
  }
  return self;
}

- (void)shortcutsItemConfigDidChange:
    (ContentSuggestionsMostVisitedActionItem*)config {
  if (config.index != _config.index) {
    return;
  }
  [self updateConfiguration:config];
}

- (void)updateConfiguration:(ContentSuggestionsMostVisitedActionItem*)config {
  _config = config;
  self.titleLabel.text = config.title;
    self.titleLabel.font = [self titleLabelFont];
  self.accessibilityTraits =
      UIAccessibilityTraitButton | config.accessibilityTraits;
  self.accessibilityLabel = config.accessibilityLabel.length
                                ? config.accessibilityLabel
                                : config.title;
  // The accessibilityUserInputLabel should just be the title, with nothing
  // extra from the accessibilityLabel.
  self.accessibilityUserInputLabels = @[ config.title ];
  self.iconView.image =
      SymbolForCollectionShortcutType(config.collectionShortcutType);
  self.countContainer.hidden = config.count == 0;
  if (config.count > 0) {
    self.countLabel.text = [@(config.count) stringValue];
  }
  self.alpha = config.disabled ? 0.3 : 1.0;
}

- (UILabel*)countLabel {
  if (!_countLabel) {
    _countContainer = [[UIView alloc] init];
    _countContainer.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    // Unfortunately, simply setting a CALayer borderWidth and borderColor
    // on `_countContainer`, and setting a background color on `_countLabel`
    // will result in the inner color bleeeding thru to the outside.
    _countContainer.layer.cornerRadius = kCountBorderWidth / 2;
    _countContainer.layer.masksToBounds = YES;

    _countLabel = [[UILabel alloc] init];
    _countLabel.layer.cornerRadius = kCountWidth / 2;
    _countLabel.layer.masksToBounds = YES;
    _countLabel.textColor = [UIColor colorNamed:kSolidButtonTextColor];
    _countLabel.textAlignment = NSTextAlignmentCenter;
    _countLabel.backgroundColor = [UIColor colorNamed:kBlueColor];

    _countContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _countLabel.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:self.countContainer];
    [self.countContainer addSubview:self.countLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_countContainer.widthAnchor constraintEqualToConstant:kCountBorderWidth],
      [_countContainer.heightAnchor
          constraintEqualToAnchor:_countContainer.widthAnchor],
      [_countContainer.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_countContainer.centerXAnchor
          constraintEqualToAnchor:self.imageContainerView.trailingAnchor],
      [_countLabel.widthAnchor constraintEqualToConstant:kCountWidth],
      [_countLabel.heightAnchor
          constraintEqualToAnchor:_countLabel.widthAnchor],
    ]];
    const CGFloat kCaptionFontSize = 12.0;
    const UIFontWeight kCaptionFontWeight = UIFontWeightRegular;
    _countLabel.font = [UIFont systemFontOfSize:kCaptionFontSize
                                         weight:kCaptionFontWeight];
    AddSameCenterConstraints(_countLabel, _countContainer);
  }
  return _countLabel;
}

#pragma mark - UIView

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    [self updateTitleLabelFontOnTraitChange];
  }
}
#endif

#pragma mark - Private

- (UIFont*)titleLabelFont {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleCaption2,
      self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityLarge);
}

// Registers a list of UITraits to observe and invokes the
// `updateTitleLabelFontOnTraitChange` function whenever one of the observed
// trait's values change.
- (void)registerViewForTraitChanges API_AVAILABLE(ios(17.0)) {
  NSArray<UITrait>* traits = TraitCollectionSetForTraits(
      @[ UITraitPreferredContentSizeCategory.self ]);
  [self registerForTraitChanges:traits
                     withAction:@selector(updateTitleLabelFontOnTraitChange)];
}

// Update the `titleLabel` font when the device's content size changes.
- (void)updateTitleLabelFontOnTraitChange {
  self.titleLabel.font = [self titleLabelFont];
}

@end
