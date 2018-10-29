// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp_tile_views/ntp_shortcut_tile_view.h"

#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

const CGFloat kCountWidth = 20;
const CGFloat kCountBorderWidth = 24;
const CGFloat kIconSize = 56;

}  // namespace

@implementation NTPShortcutTileView
@synthesize countLabel = _countLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _iconView = [[UIImageView alloc] initWithFrame:self.bounds];
    _iconView.translatesAutoresizingMaskIntoConstraints = NO;

    [self.imageContainerView addSubview:_iconView];
    AddSameConstraints(self.imageContainerView, _iconView);
    [NSLayoutConstraint activateConstraints:@[
      [_iconView.widthAnchor constraintEqualToConstant:kIconSize],
      [_iconView.heightAnchor constraintEqualToAnchor:_iconView.widthAnchor],
    ]];

    self.imageBackgroundView.tintColor =
        [UIColor colorWithRed:0.91 green:0.95 blue:0.99 alpha:1.0];
  }
  return self;
}

- (UILabel*)countLabel {
  if (!_countLabel) {
    _countContainer = [[UIView alloc] init];
    _countContainer.backgroundColor = [UIColor whiteColor];
    // Unfortunately, simply setting a CALayer borderWidth and borderColor
    // on |_countContainer|, and setting a background color on |_countLabel|
    // will result in the inner color bleeeding thru to the outside.
    _countContainer.layer.cornerRadius = kCountBorderWidth / 2;
    _countContainer.layer.masksToBounds = YES;

    _countLabel = [[UILabel alloc] init];
    _countLabel.layer.cornerRadius = kCountWidth / 2;
    _countLabel.layer.masksToBounds = YES;
    _countLabel.textColor = [UIColor whiteColor];
    _countLabel.textAlignment = NSTextAlignmentCenter;
    _countLabel.backgroundColor =
        [UIColor colorWithRed:0.10 green:0.45 blue:0.91 alpha:1.0];

    _countContainer.translatesAutoresizingMaskIntoConstraints = NO;
    _countLabel.translatesAutoresizingMaskIntoConstraints = NO;

    [self addSubview:self.countContainer];
    [self.countContainer addSubview:self.countLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_countContainer.widthAnchor constraintEqualToConstant:kCountBorderWidth],
      [_countContainer.heightAnchor
          constraintEqualToAnchor:_countContainer.widthAnchor],
      [_countContainer.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_countContainer.trailingAnchor
          constraintEqualToAnchor:self.trailingAnchor],
      [_countLabel.widthAnchor constraintEqualToConstant:kCountWidth],
      [_countLabel.heightAnchor
          constraintEqualToAnchor:_countLabel.widthAnchor],
    ]];
    AddSameCenterConstraints(_countLabel, _countContainer);
  }
  _countContainer.hidden = NO;
  return _countLabel;
}

+ (UIImage*)backgroundImage {
  // This subclass uses tintColor on the background view, so use template
  // rendering mode.
  return [[super backgroundImage]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
}

@end
