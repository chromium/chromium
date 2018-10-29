// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/favicon/favicon_view.h"

#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Default corner radius for the favicon image view.
const CGFloat kDefaultCornerRadius = 3;
}

@interface FaviconView () {
  // Property releaser for FaviconView.
}

// Image view for the favicon.
@property(nonatomic, strong) UIImageView* faviconImageView;
// Label for fallback favicon placeholder.
@property(nonatomic, strong) UILabel* faviconFallbackLabel;

@end

@implementation FaviconView
@synthesize faviconImageView = _faviconImageView;
@synthesize faviconFallbackLabel = _faviconFallbackLabel;

- (instancetype)initWithFrame:(CGRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    _faviconImageView = [[UIImageView alloc] initWithFrame:self.bounds];
    _faviconImageView.clipsToBounds = YES;
    _faviconImageView.layer.cornerRadius = kDefaultCornerRadius;
    _faviconImageView.image = nil;

    _faviconFallbackLabel = [[UILabel alloc] initWithFrame:self.bounds];
    _faviconFallbackLabel.textAlignment = NSTextAlignmentCenter;
    _faviconFallbackLabel.font = [UIFont boldSystemFontOfSize:12.0f];
    _faviconFallbackLabel.isAccessibilityElement = NO;
    _faviconFallbackLabel.clipsToBounds = YES;
    _faviconFallbackLabel.layer.cornerRadius = kDefaultCornerRadius;
    _faviconFallbackLabel.text = nil;

    [self addSubview:_faviconFallbackLabel];
    [self addSubview:_faviconImageView];

    [_faviconImageView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [_faviconFallbackLabel setTranslatesAutoresizingMaskIntoConstraints:NO];

    // Both image and fallback label are centered and match the size of favicon.
    AddSameConstraints(_faviconFallbackLabel, self);
    AddSameConstraints(_faviconImageView, self);
  }
  return self;
}

- (void)configureWithAttributes:(FaviconAttributes*)attributes {
  if (!attributes) {
    self.faviconFallbackLabel.text = nil;
    self.faviconFallbackLabel.hidden = NO;
    self.faviconImageView.hidden = YES;
    return;
  }

  if (attributes.faviconImage) {
    self.faviconImageView.image = attributes.faviconImage;
    self.faviconImageView.hidden = NO;
    self.faviconFallbackLabel.hidden = YES;
  } else {
    self.faviconFallbackLabel.backgroundColor = attributes.backgroundColor;
    self.faviconFallbackLabel.textColor = attributes.textColor;
    self.faviconFallbackLabel.text = attributes.monogramString;
    self.faviconFallbackLabel.hidden = NO;
    self.faviconImageView.hidden = YES;
  }
}

- (void)setFont:(UIFont*)font {
  self.faviconFallbackLabel.font = font;
}

#pragma mark - UIView

- (CGSize)intrinsicContentSize {
  return CGSizeMake(kFaviconPreferredSize, kFaviconPreferredSize);
}

@end
