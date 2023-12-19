// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/group_bottom_trailing_view.h"

#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
// Offsets and dimension ratio constraints of the top and bottom subviews.
const CGFloat kSubviewsDimensionRatio = 0.5;
const CGFloat kSubviewsWidthOffset = 2;
const CGFloat kSubviewsHeightOffset = 2;
}  // namespace

@interface GroupGridBottomTrailingView ()

@property(nonatomic, assign) NSInteger numberOfSubviews;
@property(nonatomic, weak) TopAlignedImageView* mainSubview;
@property(nonatomic, weak) UIImageView* topLeadingView;
@property(nonatomic, weak) UIImageView* topTrailingView;
@property(nonatomic, weak) UIImageView* bottomLeadingView;
@property(nonatomic, weak) UIImageView* bottomTrailingView;

@end

@implementation GroupGridBottomTrailingView

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    TopAlignedImageView* mainSubview = [[TopAlignedImageView alloc] init];
    mainSubview.hidden = YES;
    mainSubview.translatesAutoresizingMaskIntoConstraints = NO;

    UIImageView* topLeadingView = [self setupFaviconView];
    UIImageView* topTrailingView = [self setupFaviconView];
    UIImageView* bottomLeadingView = [self setupFaviconView];
    UIImageView* bottomTrailingView = [self setupFaviconView];

    [self addSubview:mainSubview];
    [self addSubview:topLeadingView];
    [self addSubview:topTrailingView];
    [self addSubview:bottomLeadingView];
    [self addSubview:bottomTrailingView];

    _mainSubview = mainSubview;
    _topLeadingView = topLeadingView;
    _topTrailingView = topTrailingView;
    _bottomLeadingView = bottomLeadingView;
    _bottomTrailingView = bottomTrailingView;

    // TODO(crbug.com/1501837): Apply different corner radius to each corner.
    _mainSubview.layer.cornerRadius = kGridCellCornerRadius;
    self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
    AddSameConstraints(self, mainSubview);

    NSArray* constraints = @[
      [topLeadingView.widthAnchor
          constraintEqualToAnchor:self.widthAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsWidthOffset],
      [topLeadingView.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsHeightOffset],
      [topLeadingView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [topLeadingView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],

      [topTrailingView.widthAnchor
          constraintEqualToAnchor:self.widthAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsWidthOffset],
      [topTrailingView.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsHeightOffset],
      [topTrailingView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [topTrailingView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],

      [bottomLeadingView.widthAnchor
          constraintEqualToAnchor:self.widthAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsWidthOffset],
      [bottomLeadingView.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsHeightOffset],
      [bottomLeadingView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [bottomLeadingView.leadingAnchor
          constraintEqualToAnchor:self.leadingAnchor],

      [bottomTrailingView.widthAnchor
          constraintEqualToAnchor:self.widthAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsWidthOffset],
      [bottomTrailingView.heightAnchor
          constraintEqualToAnchor:self.heightAnchor
                       multiplier:kSubviewsDimensionRatio
                         constant:-kSubviewsHeightOffset],
      [bottomTrailingView.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
      [bottomTrailingView.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],

      [topLeadingView.trailingAnchor
          constraintLessThanOrEqualToAnchor:topTrailingView.leadingAnchor],
      [topLeadingView.bottomAnchor
          constraintLessThanOrEqualToAnchor:bottomLeadingView.topAnchor],
      [topTrailingView.bottomAnchor
          constraintLessThanOrEqualToAnchor:bottomTrailingView.topAnchor],
      [bottomLeadingView.trailingAnchor
          constraintLessThanOrEqualToAnchor:bottomTrailingView.leadingAnchor],

    ];
    [NSLayoutConstraint activateConstraints:constraints];
  }

  return self;
}

- (void)setMainSubviewImageAndFavicon:
    (GroupTabInfo*)mainSubviewImageAndFavicon {
  [self hideAllViews];
  self.mainSubview.image = mainSubviewImageAndFavicon.snapshot;
  self.mainSubview.hidden = NO;
  _mainSubviewImageAndFavicon = mainSubviewImageAndFavicon;
}

- (void)setFavicons:(NSArray<UIImage*>*)favicons {
  // Start by hiding all the views as the number of visible views can change
  // depending on the number of object in `favicons`.
  [self hideAllViews];
  int faviconLength = [favicons count];

  if (faviconLength > 0) {
    self.topLeadingView.image = favicons[0];
    self.topLeadingView.hidden = NO;
  }

  if (faviconLength > 1) {
    self.topTrailingView.image = favicons[1];
    self.topTrailingView.hidden = NO;
  }

  if (faviconLength > 2) {
    self.bottomLeadingView.image = favicons[2];
    self.bottomLeadingView.hidden = NO;
  }

  if (faviconLength == 4) {
    self.bottomTrailingView.image = favicons[3];
    self.bottomTrailingView.hidden = NO;
  }

  // TODO(crbug.com/1501837): Add the bottom right view handling when the number
  // of tabs exceeds 7.

  _favicons = favicons;
}

#pragma mark - Private

- (void)hideAllViews {
  self.topLeadingView.hidden = YES;
  self.topTrailingView.hidden = YES;
  self.bottomLeadingView.hidden = YES;
  self.bottomTrailingView.hidden = YES;
  self.mainSubview.hidden = YES;
}

- (UIImageView*)setupFaviconView {
  UIImageView* imageView = [[UIImageView alloc] init];
  imageView.hidden = YES;
  // TODO(crbug.com/1501837): Apply different corner radius depending on the
  // view's position.
  imageView.layer.cornerRadius = kGroupGridBottomTrailingCellsCornerRadius;
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  return imageView;
}

@end
