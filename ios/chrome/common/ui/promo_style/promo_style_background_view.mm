// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/promo_style/promo_style_background_view.h"

#import "base/ios/ios_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

// Horizontal ratio of the left and right images in compact mode.
const CGFloat kLeftImageRatioCompact = 0.55;
const CGFloat kRightImageRatioCompact = 0.40;

// Horizontal ratio of the left and right images in regular mode.
const CGFloat kLeftImageRatioRegular = 0.35;
const CGFloat kRightImageRatioRegular = 0.25;

/// Whether the view is considered as compact.
BOOL IsCompact(UITraitCollection* traitCollection) {
  return traitCollection.horizontalSizeClass ==
             UIUserInterfaceSizeClassCompact &&
         traitCollection.verticalSizeClass != UIUserInterfaceSizeClassCompact;
}

}  // namespace

@implementation PromoStyleBackgroundView {
  UIImageView* _leftImageView;
  UIImageView* _rightImageView;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _leftImageView = [[UIImageView alloc] init];
    _leftImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _leftImageView.contentMode = UIViewContentModeTopLeft;
    [self addSubview:_leftImageView];

    _rightImageView = [[UIImageView alloc] init];
    _rightImageView.translatesAutoresizingMaskIntoConstraints = NO;
    _rightImageView.contentMode = UIViewContentModeTopRight;
    [self addSubview:_rightImageView];

    // The left and right images are RTL invariant. They are constrained to full
    // width here, the images are resized in `updateImages`.
    [NSLayoutConstraint activateConstraints:@[
      [_leftImageView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_leftImageView.rightAnchor constraintEqualToAnchor:self.rightAnchor],
      [_leftImageView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_leftImageView.leftAnchor constraintEqualToAnchor:self.leftAnchor],

      [_rightImageView.topAnchor constraintEqualToAnchor:self.topAnchor],
      [_rightImageView.rightAnchor constraintEqualToAnchor:self.rightAnchor],
      [_rightImageView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
      [_rightImageView.leftAnchor constraintEqualToAnchor:self.leftAnchor],
    ]];

    if (@available(iOS 17, *)) {
      [self registerForTraitChanges:@[
        UITraitVerticalSizeClass.self, UITraitHorizontalSizeClass.self
      ]
                         withAction:@selector(updateImagesOnNextFrame)];
    }
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  // Initial layout.
  if (!_leftImageView.image) {
    [self updateImages];
  }
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self updateImagesOnNextFrame];
}
#endif

#pragma mark - Private

/// Updates images on the next frame to get the correct bounds.
- (void)updateImagesOnNextFrame {
  __weak __typeof__(self) weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf updateImages];
  });
}

/// Returns a resized `image` with a width covering the `horizontalRatio` of the
/// view. The height is computed to keep the same aspect ratio as the original
/// image.
- (UIImage*)resizeImage:(UIImage*)image
        horizontalRatio:(CGFloat)horizontalRatio {
  CGFloat imageAspectRatio = image.size.height / image.size.width;
  const CGFloat width =
      AlignValueToPixel(self.bounds.size.width * horizontalRatio);
  const CGFloat height =
      MIN(self.bounds.size.height, AlignValueToPixel(width * imageAspectRatio));

  return ResizeImage(image, CGSizeMake(width, height),
                     ProjectionMode::kAspectFillAlignTop);
}

/// Updates left and right images.
- (void)updateImages {
  // TODO(crbug.com/40944102): Add dark mode assets.
  BOOL isCompact = IsCompact(self.traitCollection);

  UIImage* leftImage = [UIImage imageNamed:@"promo_background_left"];
  CGFloat leftImageHorizontalRatio =
      isCompact ? kLeftImageRatioCompact : kLeftImageRatioRegular;
  _leftImageView.image = [self resizeImage:leftImage
                           horizontalRatio:leftImageHorizontalRatio];

  UIImage* rightImage = [UIImage imageNamed:@"promo_background_right"];
  CGFloat rightImageHorizontalRatio =
      isCompact ? kRightImageRatioCompact : kRightImageRatioRegular;
  _rightImageView.image = [self resizeImage:rightImage
                            horizontalRatio:rightImageHorizontalRatio];
}

@end
