// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_shortcut_tile_view.h"

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_most_visited_action_item.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kCountWidth = 20;
const CGFloat kCountBorderWidth = 24;
const CGFloat kIconSize = 56;

}  // namespace

@implementation ContentSuggestionsShortcutTileView
@synthesize countLabel = _countLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame placeholder:NO];
  if (self) {
    _iconView = [[UIImageView alloc] initWithFrame:self.bounds];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.imageContainerView addSubview:_iconView];
    AddSameConstraints(self.imageContainerView, _iconView);
    [NSLayoutConstraint activateConstraints:@[
      [_iconView.widthAnchor constraintEqualToConstant:kIconSize],
      [_iconView.heightAnchor constraintEqualToAnchor:_iconView.widthAnchor],
    ]];

    self.imageBackgroundView.tintColor = [UIColor colorNamed:kBlueHaloColor];
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
  }
  return self;
}

- (void)updateConfiguration:(ContentSuggestionsMostVisitedActionItem*)config {
  _config = config;
  self.titleLabel.text = config.title;
  self.accessibilityTraits = config.accessibilityTraits;
  self.accessibilityLabel = config.accessibilityLabel.length
                                ? config.accessibilityLabel
                                : config.title;
  // The accessibilityUserInputLabel should just be the title, with nothing
  // extra from the accessibilityLabel.
  self.accessibilityUserInputLabels = @[ config.title ];
  self.iconView.image =
      UseSymbols()
          ? SymbolForCollectionShortcutType(config.collectionShortcutType)
          : ImageForCollectionShortcutType(config.collectionShortcutType);
  self.countContainer.hidden = config.count == 0;
  if (config.count > 0) {
    self.countLabel.text = [@(config.count) stringValue];
  }
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

@end
