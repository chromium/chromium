// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_view.h"

#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_tile_layout_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/public/provider/chrome/browser/raccoon/raccoon_api.h"

namespace {

const NSInteger kLabelNumLines = 2;
const CGFloat kSpaceIconTitle = 10;
const CGFloat kIconSize = 56;
const CGFloat kMagicStackIconSize = 52;
// Standard width of tiles.
const CGFloat kPreferredMaxWidth = 74;
// Image container corner radius.
const CGFloat kCornerRadius = 8.0;

}  // namespace

@interface ContentSuggestionsTileView ()
// Hold onto the created interaction for pointer support so it can be removed
// when the view goes away.
@property(nonatomic, strong) UIPointerInteraction* pointerInteraction;
@end

@implementation ContentSuggestionsTileView {
  ContentSuggestionsTileType _type;
}

- (instancetype)initWithFrame:(CGRect)frame
                     tileType:(ContentSuggestionsTileType)type {
  self = [super initWithFrame:frame];
  if (self) {
    _type = type;
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _titleLabel.font = [self titleLabelFont];
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.preferredMaxLayoutWidth = kPreferredMaxWidth;
    _titleLabel.numberOfLines = kLabelNumLines;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [self updateTitleLabelNumberOfLines];

    _imageContainerView = [[UIView alloc] init];
    _imageContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    if (ios::provider::IsRaccoonEnabled()) {
      if (@available(iOS 17.0, *)) {
        _imageContainerView.hoverStyle = [UIHoverStyle
            styleWithShape:[UIShape rectShapeWithCornerRadius:kCornerRadius]];
      }
    }

    // Use original rounded-square background image for Shorcuts regardless of
    // if it is in the Magic Stack.
    if (!IsMagicStackEnabled() ||
        type == ContentSuggestionsTileType::kShortcuts) {
      [self addSubview:_titleLabel];

      // The squircle background view.
      UIImageView* backgroundView =
          [[UIImageView alloc] initWithFrame:self.bounds];
      backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
      UIImage* backgroundImage = [[UIImage imageNamed:@"ntp_most_visited_tile"]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      backgroundView.image = backgroundImage;
      backgroundView.tintColor = [UIColor colorNamed:kGrey100Color];
      [self addSubview:backgroundView];
      [self addSubview:_imageContainerView];

      // Use smaller icon size when Shorcuts are put in Magic Stack.`
      CGFloat width = IsMagicStackEnabled() ? kMagicStackIconSize : kIconSize;
      [NSLayoutConstraint activateConstraints:@[
        [backgroundView.widthAnchor constraintEqualToConstant:width],
        [backgroundView.heightAnchor
            constraintEqualToAnchor:backgroundView.widthAnchor],
        [backgroundView.centerXAnchor
            constraintEqualToAnchor:_titleLabel.centerXAnchor],
      ]];
      AddSameCenterConstraints(_imageContainerView, backgroundView);
      UIView* containerView = backgroundView;

      ApplyVisualConstraintsWithMetrics(
          @[ @"V:|[container]-(space)-[title]|", @"H:|[title]|" ],
          @{@"container" : containerView, @"title" : _titleLabel},
          @{@"space" : @(kSpaceIconTitle)});

      _imageBackgroundView = backgroundView;
    }

    _pointerInteraction = [[UIPointerInteraction alloc] initWithDelegate:self];
    [self addInteraction:self.pointerInteraction];
  }
  return self;
}

- (void)dealloc {
  [self removeInteraction:self.pointerInteraction];
  self.pointerInteraction = nil;
}

// Returns the font size for the location label.
- (UIFont*)titleLabelFont {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleCaption1,
      self.traitCollection.preferredContentSizeCategory,
      UIContentSizeCategoryAccessibilityLarge);
}

#pragma mark - UIView

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    self.titleLabel.font = [self titleLabelFont];
    [self updateTitleLabelNumberOfLines];
  }
}

#pragma mark - UIPointerInteractionDelegate

- (UIPointerRegion*)pointerInteraction:(UIPointerInteraction*)interaction
                      regionForRequest:(UIPointerRegionRequest*)request
                         defaultRegion:(UIPointerRegion*)defaultRegion {
  return defaultRegion;
}

- (UIPointerStyle*)pointerInteraction:(UIPointerInteraction*)interaction
                       styleForRegion:(UIPointerRegion*)region {
  // The preview APIs require the view to be in a window. Ensure they are before
  // proceeding.
  if (!self.window)
    return nil;

  UITargetedPreview* preview =
      [[UITargetedPreview alloc] initWithView:_imageContainerView];
  UIPointerHighlightEffect* effect =
      [UIPointerHighlightEffect effectWithPreview:preview];
  UIPointerShape* shape =
      [UIPointerShape shapeWithRoundedRect:_imageContainerView.frame
                              cornerRadius:kCornerRadius];
  return [UIPointerStyle styleWithEffect:effect shape:shape];
}

// Updates the title label's number of rows depending on the preferred content
// size if it is in the Magic Stack since the Magic Stack has a fixed height,
// limiting the space available for multiple lines of text.
- (void)updateTitleLabelNumberOfLines {
  if (!IsMagicStackEnabled() ||
      (_type == ContentSuggestionsTileType::kMostVisited &&
       !ShouldPutMostVisitedSitesInMagicStack())) {
    return;
  }

  UIContentSizeCategory category =
      self.traitCollection.preferredContentSizeCategory;
  NSComparisonResult result = UIContentSizeCategoryCompareToCategory(
      category, UIContentSizeCategoryExtraLarge);
  if (result == NSOrderedAscending) {
    self.titleLabel.numberOfLines = kLabelNumLines;
  } else {
    self.titleLabel.numberOfLines = 1;
  }
}

@end
