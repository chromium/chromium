// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_image_view.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/ui/home_customization_framing_coordinates.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

/// Returns a new rect that is the result of rotating `rect`, `angle` (in
/// radians) around its own center.
CGRect CGRectRotatedAroundCenter(CGRect rect, CGFloat angle) {
  CGPoint center = CGPointMake(CGRectGetMidX(rect), CGRectGetMidY(rect));

  // CGAffineTransformMakeRotation rotates around (0, 0), so to rotate around
  // the rect's center, first translate the rect to its center is (0, 0), then
  // rotate, and then un-translate.
  CGAffineTransform translate_to_origin =
      CGAffineTransformMakeTranslation(-center.x, -center.y);
  CGAffineTransform rotation = CGAffineTransformMakeRotation(angle);
  CGAffineTransform translate_from_origin =
      CGAffineTransformMakeTranslation(center.x, center.y);

  CGAffineTransform t = CGAffineTransformConcat(
      translate_to_origin,
      CGAffineTransformConcat(rotation, translate_from_origin));
  return CGRectApplyAffineTransform(rect, t);
}

}  // namespace

CGRect UpdateDesiredFrame(CGRect desired_frame,
                          BOOL orientation_matches,
                          CGSize image_size) {
  // Check if the stored coordinates are same orientation as current.
  if (!orientation_matches) {
    desired_frame = CGRectRotatedAroundCenter(desired_frame, M_PI_2);
  }

  // If desired frame stretches outside the image, shrink the frame, keeping the
  // same aspect ratio.
  CGFloat frameAspectRatio =
      desired_frame.size.width / desired_frame.size.height;

  // Left inset is image.left_edge - frame.left_edge. This will end up being
  // positive if frame.left_edge is negative.
  CGFloat leftInset = MAX(0, 0 - CGRectGetMinX(desired_frame));
  CGFloat rightInset = MAX(0, CGRectGetMaxX(desired_frame) - image_size.width);
  CGFloat horizontalInset = MAX(leftInset, rightInset);

  // Left inset is image.top_edge - frame.top_edge. This will end up being
  // positive if frame.top_edge is negative.
  CGFloat topInset = MAX(0, 0 - CGRectGetMinY(desired_frame));
  CGFloat bottomInset =
      MAX(0, CGRectGetMaxY(desired_frame) - image_size.height);
  // Convert using frame aspect ratio so the two inset directions can be
  // compared.
  CGFloat verticalInset = MAX(topInset, bottomInset) * frameAspectRatio;

  CGFloat inset = MAX(horizontalInset, verticalInset);

  return CGRectInset(desired_frame, inset, inset / frameAspectRatio);
}

@interface HomeCustomizationImageView () {
  // The underlying image view to actually display the image.
  UIImageView* _imageView;

  // The framing coordinates for the current image's position.
  HomeCustomizationFramingCoordinates* _framingCoordinates;

  // Constraints used when there are no framing coordinates and the image
  // should just occupy the entire view.
  NSArray<NSLayoutConstraint*>* _fullSizeConstraints;

  // Constraints used when there are framing coordinates and teh image should
  // have a fixed size.
  NSArray<NSLayoutConstraint*>* _fixedSizeConstraints;

  // Image height constraint for the fixed size case.
  NSLayoutConstraint* _imageFixedHeightConstraint;
  // Image width constraint for the fixed size case.
  NSLayoutConstraint* _imageFixedWidthConstraint;
  // Image top margin constraint  for the fixed size case.
  NSLayoutConstraint* _imageFixedTopConstraint;
  // Image bottom margin constraint for the fixed size case.
  NSLayoutConstraint* _imageFixedLeadingConstraint;
}

@end

@implementation HomeCustomizationImageView

- (instancetype)init {
  self = [super init];
  if (self) {
    [self setupImageView];
    self.clipsToBounds = YES;
  }
  return self;
}

- (void)layoutSubviews {
  [super layoutSubviews];

  [self updateImagePosition];
}

- (UIImage*)image {
  return _imageView.image;
}

#pragma mark - View Initialization

// Configures the image view and adds it to the scroll view.
- (void)setupImageView {
  _imageView = [[UIImageView alloc] init];
  _imageView.translatesAutoresizingMaskIntoConstraints = NO;
  _imageView.contentMode = UIViewContentModeScaleAspectFill;
  [self addSubview:_imageView];

  _fullSizeConstraints = @[
    [_imageView.topAnchor constraintEqualToAnchor:self.topAnchor],
    [_imageView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    [_imageView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
    [_imageView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
  ];

  [NSLayoutConstraint activateConstraints:_fullSizeConstraints];

  _imageFixedHeightConstraint =
      [_imageView.heightAnchor constraintEqualToConstant:0];
  _imageFixedWidthConstraint =
      [_imageView.widthAnchor constraintEqualToConstant:0];
  _imageFixedTopConstraint =
      [_imageView.topAnchor constraintEqualToAnchor:self.topAnchor];
  _imageFixedLeadingConstraint =
      [_imageView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor];

  _fixedSizeConstraints = @[
    _imageFixedHeightConstraint, _imageFixedWidthConstraint,
    _imageFixedTopConstraint, _imageFixedLeadingConstraint
  ];
}

- (void)setImage:(UIImage*)image
    framingCoordinates:
        (HomeCustomizationFramingCoordinates*)framingCoordinates {
  _imageView.image = image;
  _framingCoordinates = framingCoordinates;

  if (!_imageView.image || !_framingCoordinates) {
    [NSLayoutConstraint activateConstraints:_fullSizeConstraints];
    [NSLayoutConstraint deactivateConstraints:_fixedSizeConstraints];

    return;
  }

  [self updateImagePosition];
}

/// Updates the position of the image. Even though it is positioned via
/// constraints, the position is calculated based on the view's size and
/// orientation, and this calculation cannot be done with static constraints.
- (void)updateImagePosition {
  UIImage* image = _imageView.image;

  if (!image || !_framingCoordinates) {
    return;
  }

  [NSLayoutConstraint deactivateConstraints:_fullSizeConstraints];
  [NSLayoutConstraint activateConstraints:_fixedSizeConstraints];

  // Check if the stored coordinates are same orientation as current.
  BOOL orientationMatches = _framingCoordinates.visibleRect.size.height >
                                _framingCoordinates.visibleRect.size.width ==
                            self.bounds.size.height > self.bounds.size.width;
  CGRect desiredFrame = UpdateDesiredFrame(_framingCoordinates.visibleRect,
                                           orientationMatches, image.size);

  // Calculate desired scale factor to size the image view correctly.
  CGFloat imageHeightScale = image.size.height / desiredFrame.size.height;
  CGFloat imageWidthScale = image.size.width / desiredFrame.size.width;

  CGFloat frameAspectRatio = desiredFrame.size.width / desiredFrame.size.height;
  CGFloat viewAspectRatio = self.bounds.size.width / self.bounds.size.height;

  if (frameAspectRatio <= viewAspectRatio) {
    // View is slightly shorter than desired frame, so use view's width as the
    // fixed dimension. frame will spill slightly over the bottom of the view.
    _imageFixedHeightConstraint.constant =
        self.bounds.size.width / frameAspectRatio * imageHeightScale;
    _imageFixedWidthConstraint.constant =
        self.bounds.size.width * imageWidthScale;
  } else {
    // View is slightly narrower than desired frame, so use view's height as the
    // fixed dimension. frame will spill slightly over the right of the view.
    _imageFixedHeightConstraint.constant =
        self.bounds.size.height * imageHeightScale;
    _imageFixedWidthConstraint.constant =
        self.bounds.size.height * frameAspectRatio * imageWidthScale;
  }

  // Calculate desired scale factor to convert the offset from "initial image"
  // coordinate space to view coordinate space.
  CGFloat frameToViewScale = self.bounds.size.width / desiredFrame.size.width;

  CGFloat offsetY = -desiredFrame.origin.y * frameToViewScale;
  CGFloat offsetX = -desiredFrame.origin.x * frameToViewScale;

  _imageFixedTopConstraint.constant = offsetY;
  _imageFixedLeadingConstraint.constant = offsetX;
}

@end
