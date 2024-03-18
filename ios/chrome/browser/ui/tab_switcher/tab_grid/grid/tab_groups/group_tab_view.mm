// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/tab_groups/group_tab_view.h"

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
const CGFloat kSnapshotConfigurationCornerRadius = 12;

}  // namespace

@implementation GroupTabView {
  // The view for the snapshot configuration.
  TopAlignedImageView* _snapshotFaviconView;

  // The view to display the favicon in the bottom Trailing of the
  // `_snapshotFaviconView`.
  UIView* _bottomFaviconView;

  // The view that holds the favicon image.
  UIImageView* _bottomFaviconImageView;

  // The label to display the number of remaining tabs.
  UILabel* _bottomTrailingFaviconLabel;

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
    _snapshotFaviconView = [[TopAlignedImageView alloc] init];
    _snapshotFaviconView.translatesAutoresizingMaskIntoConstraints = NO;
    _snapshotFaviconView.layer.cornerRadius =
        kSnapshotConfigurationCornerRadius;

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

    _bottomTrailingFaviconLabel = [[UILabel alloc] init];
    _bottomTrailingFaviconLabel.textColor =
        _isCell ? [UIColor colorNamed:kTextSecondaryColor]
                : [UIColor colorNamed:kTextPrimaryColor];
    _bottomTrailingFaviconLabel.translatesAutoresizingMaskIntoConstraints = NO;

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

    [_bottomFaviconView addSubview:_bottomFaviconImageView];
    [_snapshotFaviconView addSubview:_bottomFaviconView];
    [bottomTrailingFaviconView addSubview:_bottomTrailingFaviconLabel];
    AddSameCenterConstraints(bottomTrailingFaviconView,
                             _bottomTrailingFaviconLabel);
    [self addSubview:_snapshotFaviconView];

    [self addSubview:topLeadingFaviconView];
    [self addSubview:topTrailingFaviconView];
    [self addSubview:bottomLeadingFaviconView];
    [self addSubview:bottomTrailingFaviconView];

    AddSameConstraints(self, _snapshotFaviconView);
    AddSameConstraints(_snapshotFaviconView, gradientView);
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
  _snapshotFaviconView.image = snapshot;
  if (favicon && !CGSizeEqualToSize(favicon.size, CGSizeZero)) {
    _bottomFaviconImageView.image = favicon;
    _bottomFaviconView.hidden = NO;
  }
  _snapshotFaviconView.hidden = NO;
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
  _bottomTrailingFaviconLabel.hidden = NO;
  _bottomTrailingFaviconLabel.attributedText = TextForTabGroupCount(
      static_cast<int>(remainingTabsNumber), kTabGridButtonFontSize);
}

- (void)hideAllAttributes {
  _snapshotFaviconView.hidden = YES;
  _snapshotFaviconView.image = nil;
  _bottomFaviconView.hidden = YES;
  _bottomFaviconImageView.image = nil;

  for (NSUInteger i = 0; i < [_viewList count]; ++i) {
    _viewList[i].hidden = YES;
    _imageViewList[i].image = nil;
  }
  _bottomTrailingFaviconLabel.hidden = YES;
  _bottomTrailingFaviconLabel.attributedText = nil;
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
    _viewList[i].layer.cornerRadius = kGroupGridBottomTrailingCellCornerRadius;
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
