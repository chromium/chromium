// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_tab_view.h"

#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kFaviconViewScaleFactor = 0.6;
const NSInteger kTabGridButtonFontSize = 14;
const CGFloat kBottomFaviconViewWidthAndHeightAnchor = 24;
const CGFloat kBottomFaviconBottomTrailingOffset = 4;
const CGFloat kBottomFaviconViewScaleFactor = 0.75;

}  // namespace

@implementation GroupTabView {
  // The view for the snapshot configuration.
  TopAlignedImageView* _snapshotFaviconView;

  // The view to display the favicon in the bottom right of the
  // `_snapshotFaviconView`.
  UIView* _bottomFaviconView;

  // The view that holds the favicon image.
  UIImageView* _bottomFaviconImageView;

  // The views for favicon configuration.
  UIView* _faviconView;
  UIImageView* _faviconImageView;

  // The view and the label for the tabs num configuration.
  UIView* _remainingTabsView;
  UILabel* _remainingTabsLabel;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _snapshotFaviconView = [[TopAlignedImageView alloc] init];
    _snapshotFaviconView.translatesAutoresizingMaskIntoConstraints = NO;

    GradientView* gradientView = [[GradientView alloc]
        initWithTopColor:[[UIColor blackColor] colorWithAlphaComponent:0]
             bottomColor:[[UIColor blackColor] colorWithAlphaComponent:0.14]];
    gradientView.translatesAutoresizingMaskIntoConstraints = NO;
    [_snapshotFaviconView addSubview:gradientView];

    _bottomFaviconView = [[UIView alloc] init];
    _bottomFaviconView.backgroundColor = [UIColor whiteColor];
    _bottomFaviconView.layer.cornerRadius =
        kGroupGridBottomTrailingCellCornerRadius;
    _bottomFaviconView.layer.masksToBounds = YES;
    _bottomFaviconView.translatesAutoresizingMaskIntoConstraints = NO;

    _bottomFaviconImageView = [[UIImageView alloc] init];
    _bottomFaviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _bottomFaviconImageView.backgroundColor = [UIColor clearColor];
    _bottomFaviconImageView.layer.cornerRadius =
        kGroupGridFaviconViewCornerRadius;
    _bottomFaviconImageView.contentMode = UIViewContentModeScaleAspectFill;
    _bottomFaviconImageView.layer.masksToBounds = YES;

    _faviconView = [[UIView alloc] init];
    _faviconView.backgroundColor =
        [UIColor colorNamed:kTabGroupFaviconBackgroundColor];
    _faviconView.layer.cornerRadius = kGroupGridBottomTrailingCellCornerRadius;
    _faviconView.layer.masksToBounds = YES;
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    _faviconImageView = [[UIImageView alloc] init];
    _faviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _faviconImageView.backgroundColor = [UIColor clearColor];
    _faviconImageView.layer.cornerRadius = kGroupGridFaviconViewCornerRadius;
    _faviconImageView.contentMode = UIViewContentModeScaleAspectFill;
    _faviconImageView.layer.masksToBounds = YES;

    _remainingTabsView = [[UIView alloc] init];
    _remainingTabsView.backgroundColor =
        [UIColor colorNamed:kTabGroupFaviconBackgroundColor];
    _remainingTabsView.layer.cornerRadius =
        kGroupGridBottomTrailingCellCornerRadius;
    _remainingTabsView.layer.masksToBounds = YES;
    _remainingTabsView.translatesAutoresizingMaskIntoConstraints = NO;

    _remainingTabsLabel = [[UILabel alloc] init];
    _remainingTabsLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _remainingTabsLabel.translatesAutoresizingMaskIntoConstraints = NO;

    [self hideAllAttributes];

    [_bottomFaviconView addSubview:_bottomFaviconImageView];
    [_snapshotFaviconView addSubview:_bottomFaviconView];
    [_faviconView addSubview:_faviconImageView];
    [_remainingTabsView addSubview:_remainingTabsLabel];
    [self addSubview:_snapshotFaviconView];

    [self addSubview:_faviconView];
    [self addSubview:_remainingTabsView];

    AddSameConstraints(self, _snapshotFaviconView);
    AddSameConstraints(_snapshotFaviconView, gradientView);
    AddSameConstraints(self, _faviconView);
    AddSameConstraints(self, _remainingTabsView);
    AddSameCenterConstraints(_faviconView, _remainingTabsLabel);
    AddSameCenterConstraints(_faviconView, _faviconImageView);
    NSArray* constraints = @[
      [_bottomFaviconView.widthAnchor
          constraintEqualToConstant:kBottomFaviconViewWidthAndHeightAnchor],
      [_bottomFaviconView.heightAnchor
          constraintEqualToConstant:kBottomFaviconViewWidthAndHeightAnchor],
      [_bottomFaviconView.bottomAnchor
          constraintEqualToAnchor:_snapshotFaviconView.bottomAnchor
                         constant:-kBottomFaviconBottomTrailingOffset],
      [_bottomFaviconView.trailingAnchor
          constraintEqualToAnchor:_snapshotFaviconView.trailingAnchor
                         constant:-kBottomFaviconBottomTrailingOffset],

      [_bottomFaviconImageView.widthAnchor
          constraintEqualToAnchor:_bottomFaviconView.widthAnchor
                       multiplier:kBottomFaviconViewScaleFactor],
      [_bottomFaviconImageView.heightAnchor
          constraintEqualToAnchor:_bottomFaviconView.heightAnchor
                       multiplier:kBottomFaviconViewScaleFactor],

      [_bottomFaviconImageView.centerYAnchor
          constraintEqualToAnchor:_bottomFaviconView.centerYAnchor],
      [_bottomFaviconImageView.centerXAnchor
          constraintEqualToAnchor:_bottomFaviconView.centerXAnchor],

      [_faviconImageView.widthAnchor
          constraintEqualToAnchor:_faviconView.widthAnchor
                       multiplier:kFaviconViewScaleFactor],
      [_faviconImageView.heightAnchor
          constraintEqualToAnchor:_faviconImageView.widthAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }

  return self;
}

- (void)configureWithSnapshot:(UIImage*)snapshot favicon:(UIImage*)favicon {
  [self hideAllAttributes];
  _snapshotFaviconView.image = snapshot;
  _bottomFaviconImageView.image = favicon;
  _bottomFaviconImageView.hidden = NO;
  _bottomFaviconView.hidden = NO;
  _snapshotFaviconView.hidden = NO;
  return;
}

- (void)configureWithFavicon:(UIImage*)favicon {
  [self hideAllAttributes];
  _faviconImageView.image = favicon;
  _faviconImageView.hidden = NO;
  _faviconView.hidden = NO;
  return;
}

- (void)configureWithRemainingTabsNumber:(NSInteger)remainingTabsNumber {
  [self hideAllAttributes];
  if (remainingTabsNumber > 0) {
    _remainingTabsLabel.attributedText = TextForTabGroupCount(
        static_cast<int>(remainingTabsNumber), kTabGridButtonFontSize);
    _remainingTabsView.hidden = NO;
    _remainingTabsLabel.hidden = NO;
    return;
  }
}

- (void)hideAllAttributes {
  _snapshotFaviconView.hidden = YES;
  _snapshotFaviconView.image = nil;
  _bottomFaviconView.hidden = YES;
  _bottomFaviconImageView.hidden = YES;
  _bottomFaviconImageView.image = nil;
  _faviconView.hidden = YES;
  _faviconImageView.hidden = YES;
  _faviconImageView.image = nil;
  _remainingTabsView.hidden = YES;
  _remainingTabsLabel.hidden = YES;
  _remainingTabsLabel.attributedText = nil;
}

@end
