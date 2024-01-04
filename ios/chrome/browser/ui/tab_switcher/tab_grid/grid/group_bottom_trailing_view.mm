// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_bottom_trailing_view.h"

#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Offsets and dimension ratio constraints of the top and bottom subviews.
const CGFloat kSubviewsDimensionRatio = 0.5;
const CGFloat kSubviewsWidthOffset = 1;
const CGFloat kSubviewsHeightOffset = 1;
const NSInteger kTabGridButtonFontSize = 14;
}  // namespace

@implementation GroupGridBottomTrailingView {
  // The view to display for 1 tab configuration.
  TopAlignedImageView* _mainSubview;
  // The views to display  if the number of tabs exceeds 1.
  TopAlignedImageView* _topLeadingView;
  TopAlignedImageView* _topTrailingView;
  TopAlignedImageView* _bottomLeadingView;
  TopAlignedImageView* _bottomTrailingView;
  // The label to display the number of remaining tabs.
  UILabel* _remainingTabsLabel;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _mainSubview = [[TopAlignedImageView alloc] init];
    _mainSubview.hidden = YES;
    _mainSubview.translatesAutoresizingMaskIntoConstraints = NO;

    _topLeadingView = [self setupFaviconView];
    _topTrailingView = [self setupFaviconView];
    _bottomLeadingView = [self setupFaviconView];
    _bottomTrailingView = [self setupFaviconView];
    _remainingTabsLabel = [[UILabel alloc] init];
    _remainingTabsLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _remainingTabsLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _remainingTabsLabel.hidden = YES;
    [_bottomTrailingView addSubview:_remainingTabsLabel];

    [self addSubview:_mainSubview];
    [self addSubview:_topLeadingView];
    [self addSubview:_topTrailingView];
    [self addSubview:_bottomLeadingView];
    [self addSubview:_bottomTrailingView];

    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    AddSameConstraints(self, _mainSubview);

    NSArray* constraints = @[
      [_topLeadingView.widthAnchor
          constraintEqualToAnchor:self.widthAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsWidthOffset],
      [_topLeadingView.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsHeightOffset],
      [_topLeadingView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_topLeadingView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],

      [_topTrailingView.widthAnchor
          constraintEqualToAnchor:self.widthAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsWidthOffset],
      [_topTrailingView.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsHeightOffset],
      [_topTrailingView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_topTrailingView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],

      [_bottomLeadingView.widthAnchor
          constraintEqualToAnchor:self.widthAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsWidthOffset],
      [_bottomLeadingView.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsHeightOffset],
      [_bottomLeadingView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [_bottomLeadingView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],

      [_bottomTrailingView.widthAnchor
          constraintEqualToAnchor:self.widthAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsWidthOffset],
      [_bottomTrailingView.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsHeightOffset],
      [_bottomTrailingView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [_bottomTrailingView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],

      [_topLeadingView.trailingAnchor
          constraintLessThanOrEqualToAnchor:_topTrailingView.leadingAnchor],
      [_topLeadingView.bottomAnchor
          constraintLessThanOrEqualToAnchor:_bottomLeadingView.topAnchor],
      [_topTrailingView.bottomAnchor
          constraintLessThanOrEqualToAnchor:_bottomTrailingView.topAnchor],
      [_bottomLeadingView.trailingAnchor
          constraintLessThanOrEqualToAnchor:_bottomTrailingView.leadingAnchor],
      [_remainingTabsLabel.centerYAnchor
          constraintEqualToAnchor:_bottomTrailingView.centerYAnchor],
      [_remainingTabsLabel.centerXAnchor
          constraintEqualToAnchor:_bottomTrailingView.centerXAnchor],

    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }

  return self;
}

- (void)configureWithGroupTabInfo:(GroupTabInfo*)groupTabInfo {
  [self hideAllViews];
  _mainSubview.image = groupTabInfo.snapshot;
  _mainSubview.hidden = NO;
}

- (void)configureWithFavicons:(NSArray<UIImage*>*)favicons
           remainingTabsCount:(NSInteger)remainingTabsCount {
  // Start by hiding all the views as the number of visible views can change
  // depending on the number of object in `favicons`.
  [self hideAllViews];

  int faviconLength = [favicons count];

  if (faviconLength > 0) {
    _topLeadingView.image = favicons[0];
    _topLeadingView.hidden = NO;
  }

  if (faviconLength > 1) {
    _topTrailingView.image = favicons[1];
    _topTrailingView.hidden = NO;
  }

  if (faviconLength > 2) {
    _bottomLeadingView.image = favicons[2];
    _bottomLeadingView.hidden = NO;
  }

  if (faviconLength == 4) {
    _bottomTrailingView.image = favicons[3];
    _bottomTrailingView.hidden = NO;
  }

  if (remainingTabsCount > 0) {
    [self setupBottomTrailingViewRemainingTabsCount:remainingTabsCount];
  }
}

#pragma mark - Private

- (void)hideAllViews {
  _topLeadingView.hidden = YES;
  _topTrailingView.hidden = YES;
  _bottomLeadingView.hidden = YES;
  _bottomTrailingView.hidden = YES;
  _mainSubview.hidden = YES;
  _remainingTabsLabel.hidden = YES;
}

- (TopAlignedImageView*)setupFaviconView {
  TopAlignedImageView* imageView = [[TopAlignedImageView alloc] init];
  imageView.hidden = YES;
  imageView.layer.cornerRadius = kGroupGridBottomTrailingCellCornerRadius;
  // TODO(crbug.com/1501837): Add the shadows.
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  return imageView;
}

- (void)setupBottomTrailingViewRemainingTabsCount:
    (NSInteger)remainingTabsCount {
  _bottomTrailingView.image = nil;
  _remainingTabsLabel.attributedText =
      TextForTabGroupCount(int(remainingTabsCount), kTabGridButtonFontSize);
  _remainingTabsLabel.hidden = NO;
  _bottomTrailingView.hidden = NO;
}

@end
