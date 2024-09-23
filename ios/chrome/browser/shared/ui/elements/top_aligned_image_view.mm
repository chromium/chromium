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
  if ((self = [super initWithFrame:CGRectZero])) {
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
  const CGFloat imageAspectRatio = imageSize.width / imageSize.height;
  CGFloat imageViewWidth;
  CGFloat imageViewHeight;
  if (imageSize.width > imageSize.height) {
    // The image is landscape. Adapt the image view size based on the difference
    // of aspect ratios.
    const CGFloat viewAspectRatio =
        CGRectGetWidth(self.bounds) / CGRectGetHeight(self.bounds);
    if (imageAspectRatio > viewAspectRatio) {
      // The image is wider than the view. Fit the height.
      imageViewHeight = CGRectGetHeight(self.bounds);
      imageViewWidth = CGRectGetHeight(self.bounds) * imageAspectRatio;
    } else {
      // The image is narrower than the view. Fit the width.
      imageViewWidth = CGRectGetWidth(self.bounds);
      imageViewHeight = CGRectGetWidth(self.bounds) / imageAspectRatio;
    }
  } else {
    // The image is portrait. Always match the width, even if it leads to a
    // white bar at the bottom if the view is narrower than the image.
    // See header for the explanation.
    imageViewWidth = CGRectGetWidth(self.bounds);
    imageViewHeight = CGRectGetWidth(self.bounds) / imageAspectRatio;
  }
  self.innerImageView.frame = CGRectMake(
      // Always center horizontally.
      (CGRectGetWidth(self.bounds) - imageViewWidth) / 2.,
      // Always align to the top.
      0,
      // Set the computed image view size.
      imageViewWidth, imageViewHeight);
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
