// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_view.h"

#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const NSInteger kLabelNumLines = 2;
const CGFloat kSpaceIconTitle = 10;
const CGFloat kIconSize = 56;
const CGFloat kTitleAlpha = 0.54;
const CGFloat kPreferredMaxWidth = 73;

}  // namespace

@implementation NTPTileView

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.textColor = [UIColor colorWithWhite:0 alpha:kTitleAlpha];
    _titleLabel.font = [UIFont systemFontOfSize:12];
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.preferredMaxLayoutWidth = kPreferredMaxWidth;
    _titleLabel.numberOfLines = kLabelNumLines;

    _imageContainerView = [[UIView alloc] init];
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _imageContainerView.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:_titleLabel];

    // The squircle background view.
    UIImageView* backgroundView =
        [[UIImageView alloc] initWithFrame:self.bounds];
    backgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    backgroundView.image = [[self class] backgroundImage];
    [self addSubview:backgroundView];
    [self addSubview:_imageContainerView];

    [NSLayoutConstraint activateConstraints:@[
      [backgroundView.widthAnchor constraintEqualToConstant:kIconSize],
      [backgroundView.heightAnchor
          constraintEqualToAnchor:backgroundView.widthAnchor],
      [backgroundView.centerXAnchor
          constraintEqualToAnchor:_titleLabel.centerXAnchor],
    ]];
    AddSameCenterConstraints(_imageContainerView, backgroundView);
    UIView* containerView = backgroundView;

    ApplyVisualConstraintsWithMetrics(
        @[ @"V:|[container]-(space)-[title]", @"H:|[title]|" ],
        @{@"container" : containerView, @"title" : _titleLabel},
        @{ @"space" : @(kSpaceIconTitle) });

    _imageBackgroundView = backgroundView;
  }
  return self;
}

+ (UIImage*)backgroundImage {
  return [UIImage imageNamed:@"ntp_most_visited_tile"];
}

@end
