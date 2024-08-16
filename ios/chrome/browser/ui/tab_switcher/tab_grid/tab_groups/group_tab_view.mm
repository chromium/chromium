// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_groups/group_tab_view.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/gradient_view.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

const CGFloat kFaviconViewScaleFactor = 0.5;
const NSInteger kTabGridButtonFontSize = 14;
const CGFloat kBottomFaviconViewWidthAndHeightAnchor = 24;
const CGFloat kBottomFaviconBottomTrailingOffset = 4;
const CGFloat kBottomFaviconViewScaleFactor = 0.75;
const CGFloat kFaviconSpacing = 2.0;
const CGFloat kTabViewCornerRadius = 12;
const CGFloat kFaviconCornerRadius = 8;

}  // namespace

@implementation GroupTabView {
  // The view for the snapshot configuration.
  TopAlignedImageView* _snapshotView;

  // The view to display the favicon in the bottom Trailing of the
  // `_snapshotView`.
  UIView* _snapshotFaviconView;

  // The view that holds the favicon image.
  UIImageView* _snapshotFaviconImageView;

  // The label to display the number of remaining tabs.
  UILabel* _bottomTrailingLabel;

  // List of the view that compose the squared layout.
  NSArray<UIView*>* _viewList;
  NSArray<UIImageView*>* _imageViewList;

  // YES if this view is displayed in a cell.
  BOOL _isCell;
}

- (instancetype)initWithIsCell:(BOOL)isCell {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _isCell = isCell;

    self.layer.cornerRadius = kTabViewCornerRadius;
    self.layer.masksToBounds = YES;

    _snapshotView = [[TopAlignedImageView alloc] init];
    _snapshotView.translatesAutoresizingMaskIntoConstraints = NO;

    GradientView* gradientView = [[GradientView alloc]
        initWithTopColor:[[UIColor blackColor] colorWithAlphaComponent:0]
             bottomColor:[[UIColor blackColor] colorWithAlphaComponent:0.14]];
    gradientView.translatesAutoresizingMaskIntoConstraints = NO;
    [_snapshotView addSubview:gradientView];

    _snapshotFaviconView = [[UIView alloc] init];
    _snapshotFaviconView.backgroundColor = [UIColor whiteColor];
    _snapshotFaviconView.layer.cornerRadius = kFaviconCornerRadius;
    _snapshotFaviconView.layer.masksToBounds = YES;
    _snapshotFaviconView.translatesAutoresizingMaskIntoConstraints = NO;

    _snapshotFaviconImageView = [[UIImageView alloc] init];
    _snapshotFaviconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _snapshotFaviconImageView.backgroundColor = [UIColor clearColor];
    _snapshotFaviconImageView.layer.cornerRadius =
        kGroupGridFaviconViewCornerRadius;
    _snapshotFaviconImageView.contentMode = UIViewContentModeScaleAspectFill;
    _snapshotFaviconImageView.layer.masksToBounds = YES;

    _bottomTrailingLabel = [[UILabel alloc] init];
    _bottomTrailingLabel.textColor =
        _isCell ? [UIColor colorNamed:kTextSecondaryColor]
                : [UIColor colorNamed:kTextPrimaryColor];
    _bottomTrailingLabel.translatesAutoresizingMaskIntoConstraints = NO;

    UIView* topLeadingFaviconView = [[UIView alloc] init];
    UIView* topTrailingFaviconView = [[UIView alloc] init];
    UIView* bottomLeadingFaviconView = [[UIView alloc] init];
    UIView* bottomTrailingFaviconView = [[UIView alloc] init];
    UIImageView* topLeadingFaviconImageView = [[UIImageView alloc] init];
    UIImageView* topTrailingFaviconImageView = [[UIImageView alloc] init];
    UIImageView* bottomLeadingFaviconImageView = [[UIImageView alloc] init];
    UIImageView* bottomTrailingFaviconImageView = [[UIImageView alloc] init];

    _viewList = @[
      topLeadingFaviconView, topTrailingFaviconView, bottomLeadingFaviconView,
      bottomTrailingFaviconView
    ];
    _imageViewList = @[
      topLeadingFaviconImageView, topTrailingFaviconImageView,
      bottomLeadingFaviconImageView, bottomTrailingFaviconImageView
    ];

    [self configureFaviconViewAndAssociatedImageView];

    [self hideAllAttributes];

    [_snapshotFaviconView addSubview:_snapshotFaviconImageView];
    [_snapshotView addSubview:_snapshotFaviconView];
    [bottomTrailingFaviconView addSubview:_bottomTrailingLabel];
    AddSameCenterConstraints(bottomTrailingFaviconView, _bottomTrailingLabel);
    [self addSubview:_snapshotView];

    [self addSubview:topLeadingFaviconView];
    [self addSubview:topTrailingFaviconView];
    [self addSubview:bottomLeadingFaviconView];
    [self addSubview:bottomTrailingFaviconView];

    AddSameConstraints(self, _snapshotView);
    AddSameConstraints(_snapshotView, gradientView);
    NSArray* constraints = @[
      [_snapshotFaviconView.widthAnchor
          constraintEqualToConstant:kBottomFaviconViewWidthAndHeightAnchor],
      [_snapshotFaviconView.heightAnchor
          constraintEqualToConstant:kBottomFaviconViewWidthAndHeightAnchor],
      [_snapshotFaviconView.bottomAnchor
          constraintEqualToAnchor:_snapshotView.bottomAnchor
                         constant:-kBottomFaviconBottomTrailingOffset],
      [_snapshotFaviconView.trailingAnchor
          constraintEqualToAnchor:_snapshotView.trailingAnchor
                         constant:-kBottomFaviconBottomTrailingOffset],

      [_snapshotFaviconImageView.widthAnchor
          constraintEqualToAnchor:_snapshotFaviconView.widthAnchor
                       multiplier:kBottomFaviconViewScaleFactor],
      [_snapshotFaviconImageView.heightAnchor
          constraintEqualToAnchor:_snapshotFaviconView.heightAnchor
                       multiplier:kBottomFaviconViewScaleFactor],

      [_snapshotFaviconImageView.centerYAnchor
          constraintEqualToAnchor:_snapshotFaviconView.centerYAnchor],
      [_snapshotFaviconImageView.centerXAnchor
          constraintEqualToAnchor:_snapshotFaviconView.centerXAnchor],

      [topLeadingFaviconView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [topLeadingFaviconView.topAnchor constraintEqualToAnchor:self.topAnchor],

      [topTrailingFaviconView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [topTrailingFaviconView.topAnchor constraintEqualToAnchor:self.topAnchor],

      [bottomLeadingFaviconView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],
      [bottomLeadingFaviconView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],

      [bottomTrailingFaviconView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [bottomTrailingFaviconView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],

      [topTrailingFaviconView.leadingAnchor
          constraintEqualToAnchor:topLeadingFaviconView.trailingAnchor
                         constant:kFaviconSpacing],
      [topLeadingFaviconView.widthAnchor
          constraintEqualToAnchor:topTrailingFaviconView.widthAnchor],

      [bottomTrailingFaviconView.leadingAnchor
          constraintEqualToAnchor:bottomLeadingFaviconView.trailingAnchor
                         constant:kFaviconSpacing],
      [bottomLeadingFaviconView.widthAnchor
          constraintEqualToAnchor:bottomTrailingFaviconView.widthAnchor],

      [bottomLeadingFaviconView.topAnchor
          constraintEqualToAnchor:topLeadingFaviconView.bottomAnchor
                         constant:kFaviconSpacing],
      [bottomLeadingFaviconView.heightAnchor
          constraintEqualToAnchor:topLeadingFaviconView.heightAnchor],

      [bottomTrailingFaviconView.topAnchor
          constraintEqualToAnchor:topTrailingFaviconView.bottomAnchor
                         constant:kFaviconSpacing],
      [bottomTrailingFaviconView.heightAnchor
          constraintEqualToAnchor:topTrailingFaviconView.heightAnchor],
    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }

  return self;
}

- (void)configureWithSnapshot:(UIImage*)snapshot favicon:(UIImage*)favicon {
  [self hideAllAttributes];
  _snapshotView.image = snapshot;
  if (favicon && !CGSizeEqualToSize(favicon.size, CGSizeZero)) {
    _snapshotFaviconImageView.image = favicon;
    _snapshotFaviconView.hidden = NO;
  }
  _snapshotView.hidden = NO;
}

- (void)configureWithFavicons:(NSArray<UIImage*>*)favicons {
  [self hideAllAttributes];
  CHECK_LE([favicons count], [_viewList count]);

  for (NSUInteger i = 0; i < [favicons count]; ++i) {
    _viewList[i].hidden = NO;
    _imageViewList[i].hidden = NO;
    _imageViewList[i].image = favicons[i];
  }
}

- (void)configureWithFavicons:(NSArray<UIImage*>*)favicons
          remainingTabsNumber:(NSInteger)remainingTabsNumber {
  [self configureWithFavicons:favicons];
  _viewList[3].hidden = NO;
  _bottomTrailingLabel.hidden = NO;
  _bottomTrailingLabel.attributedText = TextForTabGroupCount(
      static_cast<int>(remainingTabsNumber), kTabGridButtonFontSize);
}

- (void)hideAllAttributes {
  _snapshotView.hidden = YES;
  _snapshotView.image = nil;
  _snapshotFaviconView.hidden = YES;
  _snapshotFaviconImageView.image = nil;

  for (NSUInteger i = 0; i < [_viewList count]; ++i) {
    _viewList[i].hidden = YES;
    _imageViewList[i].image = nil;
  }
  _bottomTrailingLabel.hidden = YES;
  _bottomTrailingLabel.attributedText = nil;
  self.hidden = NO;
}

#pragma mark - Private

// Configures the views from `_viewList` with their associated image views from
// `_imageViewList`.
- (void)configureFaviconViewAndAssociatedImageView {
  for (NSUInteger i = 0; i < [_viewList count]; ++i) {
    if (_isCell) {
      _viewList[i].backgroundColor =
          [UIColor colorNamed:kTabGroupFaviconBackgroundColor];
    } else {
      _viewList[i].backgroundColor = [UIColor colorWithWhite:1.0 alpha:0.5];
    }
    _viewList[i].layer.cornerRadius = kFaviconCornerRadius;
    _viewList[i].layer.masksToBounds = YES;
    _viewList[i].translatesAutoresizingMaskIntoConstraints = NO;

    _imageViewList[i].translatesAutoresizingMaskIntoConstraints = NO;
    _imageViewList[i].backgroundColor = [UIColor clearColor];
    _imageViewList[i].layer.cornerRadius = kGroupGridFaviconViewCornerRadius;
    _imageViewList[i].contentMode = UIViewContentModeScaleAspectFill;
    _imageViewList[i].layer.masksToBounds = YES;

    [_viewList[i] addSubview:_imageViewList[i]];
    AddSameCenterConstraints(_viewList[i], _imageViewList[i]);

    [NSLayoutConstraint activateConstraints:@[
      [_imageViewList[i].widthAnchor
          constraintEqualToAnchor:_viewList[i].widthAnchor
                       multiplier:kFaviconViewScaleFactor],
      [_imageViewList[i].heightAnchor
          constraintEqualToAnchor:_imageViewList[i].widthAnchor],
    ]];
  }
}

@end
