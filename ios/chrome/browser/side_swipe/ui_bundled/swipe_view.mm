// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/side_swipe/ui_bundled/swipe_view.h"

#import "ios/chrome/browser/shared/ui/elements/top_aligned_image_view.h"
#import "ios/web/common/features.h"

@interface SwipeView ()

@property(nonatomic, strong) UIImageView* topToolbarSnapshot;
@property(nonatomic, strong) UIImageView* bottomToolbarSnapshot;

@property(nonatomic, strong) NSLayoutConstraint* toolbarTopConstraint;
@property(nonatomic, strong) NSLayoutConstraint* imageTopConstraint;

@property(nonatomic, strong) TopAlignedImageView* imageView;

@end

@implementation SwipeView

@synthesize topToolbarSnapshot = _topToolbarSnapshot;
@synthesize bottomToolbarSnapshot = _bottomToolbarSnapshot;
@synthesize topMargin = _topMargin;
@synthesize toolbarTopConstraint = _toolbarTopConstraint;
@synthesize imageTopConstraint = _imageTopConstraint;
@synthesize imageView = _imageView;

- (instancetype)initWithFrame:(CGRect)frame topMargin:(CGFloat)topMargin {
  self = [super initWithFrame:frame];
  if (self) {
    _topMargin = topMargin;

    _imageView = [[TopAlignedImageView alloc] init];
    [_imageView setBackgroundColor:[UIColor whiteColor]];
    [self addSubview:_imageView];

    _topToolbarSnapshot = [[UIImageView alloc] initWithFrame:CGRectZero];
    [self addSubview:_topToolbarSnapshot];

    _bottomToolbarSnapshot = [[UIImageView alloc] initWithFrame:CGRectZero];
    [self addSubview:_bottomToolbarSnapshot];

    // All subviews are as wide as the parent
    NSMutableArray* constraints = [NSMutableArray array];
    for (UIView* view in self.subviews) {
      [view setTranslatesAutoresizingMaskIntoConstraints:NO];
      [constraints addObject:[view.leadingAnchor
                                 constraintEqualToAnchor:self.leadingAnchor]];
      [constraints addObject:[view.trailingAnchor
                                 constraintEqualToAnchor:self.trailingAnchor]];
    }

    _toolbarTopConstraint = [[_topToolbarSnapshot topAnchor]
        constraintEqualToAnchor:self.topAnchor];

    _imageTopConstraint =
        [_imageView.topAnchor constraintEqualToAnchor:self.topAnchor
                                             constant:topMargin];
    [constraints addObjectsFromArray:@[
      _imageTopConstraint,
      [[_imageView bottomAnchor] constraintEqualToAnchor:self.bottomAnchor],
      _toolbarTopConstraint,
      [_bottomToolbarSnapshot.bottomAnchor
          constraintEqualToAnchor:self.bottomAnchor],
    ]];

    [NSLayoutConstraint activateConstraints:constraints];
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];
  [self updateImageBoundsAndZoom];
}

- (void)updateImageBoundsAndZoom {
  UIImage* image = self.imageView.image;
  if (image) {
    CGSize imageSize = image.size;
    CGSize viewSize = self.imageView.frame.size;
    CGFloat zoomRatio = std::max(viewSize.height / imageSize.height,
                                 viewSize.width / imageSize.width);
    self.imageView.layer.contentsRect =
        CGRectMake(0.0, 0.0, viewSize.width / (zoomRatio * imageSize.width),
                   viewSize.height / (zoomRatio * imageSize.height));
  }
}

- (void)setTopMargin:(CGFloat)topMargin {
  _topMargin = topMargin;
  self.imageTopConstraint.constant = topMargin;
}

- (void)setImage:(UIImage*)image {
  self.imageView.image = image;
  [self updateImageBoundsAndZoom];
}

- (void)setTopToolbarImage:(UIImage*)image {
  [self.topToolbarSnapshot setImage:image];
  [self.topToolbarSnapshot setNeedsLayout];
}

- (void)setBottomToolbarImage:(UIImage*)image {
  [self.bottomToolbarSnapshot setImage:image];
  [self.bottomToolbarSnapshot setNeedsLayout];
}

@end
