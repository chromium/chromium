// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"

@interface TopAlignedImageView ()
// The backing image view.
@property(nonatomic, weak) UIImageView* innerImageView;
@end

@implementation TopAlignedImageView
@synthesize innerImageView = _innerImageView;

- (instancetype)init {
  if (self = [super initWithFrame:CGRectZero]) {
    UIImageView* innerImageView = [[UIImageView alloc] init];
    [self addSubview:innerImageView];
    _innerImageView = innerImageView;
    _innerImageView.contentMode = UIViewContentModeScaleAspectFill;
    _innerImageView.backgroundColor = [UIColor clearColor];
    self.clipsToBounds = YES;
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  const CGSize imageSize = self.image.size;
  if (imageSize.width == 0 || imageSize.height == 0) {
    return;
  }
  CGFloat widthScaleFactor = CGRectGetWidth(self.bounds) / imageSize.width;
  CGFloat imageViewWidth = CGRectGetWidth(self.bounds);
  CGFloat imageViewHeight = imageSize.height * widthScaleFactor;
  self.innerImageView.frame = CGRectMake(0, 0, imageViewWidth, imageViewHeight);
}

#pragma mark - Public properties

- (void)setImage:(UIImage*)image {
  self.innerImageView.image = image;
  [self setNeedsLayout];
}

- (UIImage*)image {
  return self.innerImageView.image;
}

@end
