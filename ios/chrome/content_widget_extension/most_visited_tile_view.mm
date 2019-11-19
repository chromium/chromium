// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/content_widget_extension/most_visited_tile_view.h"

#import <NotificationCenter/NotificationCenter.h>

#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const NSInteger kLabelNumLines = 2;
const CGFloat kFaviconSize = 48;
const CGFloat kSpaceFaviconTitle = 8;

// Width of a tile.
const CGFloat kTileWidth = 73;
}

@implementation MostVisitedTileView

@synthesize titleLabel = _titleLabel;

#pragma mark - Public

+ (CGFloat)tileWidth {
  return kTileWidth;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    UIVibrancyEffect* labelEffect =
        [UIVibrancyEffect widgetSecondaryVibrancyEffect];
    if (@available(iOS 13, *)) {
      labelEffect = [UIVibrancyEffect
          widgetEffectForVibrancyStyle:UIVibrancyEffectStyleSecondaryLabel];
    }

    UIVisualEffectView* titleLabelEffectView =
        [[UIVisualEffectView alloc] initWithEffect:labelEffect];
    titleLabelEffectView.translatesAutoresizingMaskIntoConstraints = NO;

    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2];
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.isAccessibilityElement = NO;
    _titleLabel.numberOfLines = kLabelNumLines;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    [titleLabelEffectView.contentView addSubview:_titleLabel];
    AddSameConstraints(titleLabelEffectView, _titleLabel);

    _faviconView = [[FaviconView alloc] init];
    _faviconView.isAccessibilityElement = NO;
    _faviconView.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    _faviconView.translatesAutoresizingMaskIntoConstraints = NO;

    UIStackView* stack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _faviconView, titleLabelEffectView ]];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = kSpaceFaviconTitle;
    stack.alignment = UIStackViewAlignmentCenter;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    stack.isAccessibilityElement = NO;
    stack.userInteractionEnabled = NO;
    [self addSubview:stack];
    AddSameConstraints(self, stack);

    [NSLayoutConstraint activateConstraints:@[
      [stack.widthAnchor constraintEqualToConstant:kTileWidth],
      [_faviconView.widthAnchor constraintEqualToConstant:kFaviconSize],
      [_faviconView.heightAnchor constraintEqualToConstant:kFaviconSize],
    ]];

    self.translatesAutoresizingMaskIntoConstraints = NO;

    self.highlightableViews = @[ _faviconView, _titleLabel ];
  }
  return self;
}

@end
