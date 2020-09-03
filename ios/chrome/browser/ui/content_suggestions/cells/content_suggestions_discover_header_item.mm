// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_discover_header_item.h"

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Leading and trailing margin for label and button.
const CGFloat kHeaderHorizontalMargin = 20;
// Font size for label text in header.
const CGFloat kDiscoverFeedTitleFontSize = 16;
// Insets for header menu button.
const CGFloat kHeaderMenuButtonInsetTopAndBottom = 2;
const CGFloat kHeaderMenuButtonInsetSides = 2;
// Duration for the header animation when Discover feed visibility changes.
const CGFloat kHeaderChangeAnimationDuration = 0.3;
}

#pragma mark - ContentSuggestionsDiscoverHeaderItem

@implementation ContentSuggestionsDiscoverHeaderItem

- (instancetype)initWithType:(NSInteger)type discoverFeedVisible:(BOOL)visible {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [ContentSuggestionsDiscoverHeaderCell class];
    _discoverFeedVisible = visible;
  }
  return self;
}

- (void)configureCell:(ContentSuggestionsDiscoverHeaderCell*)cell {
  [super configureCell:cell];
  cell.title = self.title;
  [cell changeHeaderForFeedVisible:self.discoverFeedVisible];
}

@end

#pragma mark - ContentSuggestionsDiscoverHeaderCell

@interface ContentSuggestionsDiscoverHeaderCell ()

// Represents whether the Discover feed is visible or hidden. NSNumber allows
// for nil value before being set.
@property(nonatomic) NSNumber* discoverFeedVisible;

// Header constraints for when the feed is visible.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* feedVisibleConstraints;

// Header constraints for when the feed is hidden.
@property(nonatomic, strong)
    NSArray<NSLayoutConstraint*>* feedHiddenConstraints;

// Title label for the feed.
@property(nonatomic, strong) UILabel* titleLabel;

@end

@implementation ContentSuggestionsDiscoverHeaderCell

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {

    _titleLabel = [[UILabel alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.font = [UIFont systemFontOfSize:kDiscoverFeedTitleFontSize
                                         weight:UIFontWeightMedium];
    _titleLabel.textColor = [UIColor colorNamed:kGrey700Color];
    _titleLabel.adjustsFontForContentSizeCategory = YES;

    _menuButton = [[UIButton alloc] init];
    _menuButton.translatesAutoresizingMaskIntoConstraints = NO;
    _menuButton.accessibilityIdentifier =
        kContentSuggestionsDiscoverHeaderButtonIdentifier;
    _menuButton.accessibilityLabel =
        l10n_util::GetNSString(IDS_IOS_DISCOVER_FEED_MENU_ACCESSIBILITY_LABEL);
    [_menuButton
        setImage:[[UIImage imageNamed:@"infobar_settings_icon"]
                     imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate]
        forState:UIControlStateNormal];
    _menuButton.tintColor = [UIColor colorNamed:kGrey600Color];
    _menuButton.imageEdgeInsets = UIEdgeInsetsMake(
        kHeaderMenuButtonInsetTopAndBottom, kHeaderMenuButtonInsetSides,
        kHeaderMenuButtonInsetTopAndBottom, kHeaderMenuButtonInsetSides);

    [self.contentView addSubview:_menuButton];
    [self.contentView addSubview:_titleLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_titleLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHeaderHorizontalMargin],
      [_titleLabel.trailingAnchor
          constraintLessThanOrEqualToAnchor:_menuButton.leadingAnchor],
      [_menuButton.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHeaderHorizontalMargin],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self.menuButton removeTarget:nil
                         action:nil
               forControlEvents:UIControlEventAllEvents];
}

- (void)changeHeaderForFeedVisible:(BOOL)visible {
  // Checks is discoverFeedVisible value is nil, indicating that the header has
  // been newly created or reloaded.
  if (self.discoverFeedVisible) {
    if ([self.discoverFeedVisible boolValue] == visible) {
      return;
    }
    [self setHeaderForFeedVisible:visible animate:YES];
  } else {
    [self setHeaderForFeedVisible:visible animate:NO];
  }
  [self.contentView layoutIfNeeded];
  NamedGuide* menuButtonGuide =
      [NamedGuide guideWithName:kDiscoverFeedHeaderMenuGuide
                           view:self.menuButton];

  menuButtonGuide.constrainedFrame =
      [self.contentView convertRect:self.menuButton.frame toView:nil];
  self.discoverFeedVisible = [NSNumber numberWithBool:visible];
}

#pragma mark - Private

- (void)setHeaderForFeedVisible:(BOOL)visible animate:(BOOL)animate {
  if (animate) {
    [UIView transitionWithView:self.titleLabel
                      duration:kHeaderChangeAnimationDuration
                       options:UIViewAnimationOptionTransitionCrossDissolve
                    animations:^{
                      [self setHeaderTitleForFeedVisible:visible];
                    }
                    completion:nil];
  } else {
    [self setHeaderTitleForFeedVisible:visible];
  }
}

- (void)setHeaderTitleForFeedVisible:(BOOL)visible {
  self.titleLabel.text =
      visible
          ? self.title
          : [NSString
                stringWithFormat:@"%@ – %@", self.title,
                                 l10n_util::GetNSString(
                                     IDS_IOS_DISCOVER_FEED_TITLE_OFF_LABEL)];
}

@end
