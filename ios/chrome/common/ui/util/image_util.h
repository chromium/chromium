// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_IMAGE_UTIL_H_
#define IOS_CHROME_COMMON_UI_UTIL_IMAGE_UTIL_H_

#import <UIKit/UIKit.h>

// This function is used to figure out how to resize an image from an
// `originalSize` to a `targetSize`. It returns a `revisedTargetSize` of the
// resized  image and `projectTo` that is used to describe the rectangle in the
// target that the image will be covering. Returned values are always floored to
// integral values.
//
// The ProjectionMode describes in which way the stretching will apply.
//
enum class ProjectionMode {
  // Just stretches the source into the destination, not preserving aspect ratio
  // at all.
  // `projectTo` and `revisedTargetSize` will be set to `targetSize`
  kFill,

  // Scale to the target, maintaining aspect ratio, clipping the excess, while
  // keeping the image centered.
  // Large original sizes are shrunk until they fit on one side, small original
  // sizes are expanded.
  // `projectTo` will be a subset of `originalSize`
  // `revisedTargetSize` will be set to `targetSize`
  kAspectFill,

  // Same as kAspectFill, except that the bottom part of the image will be
  // clipped. The image will still be horizontally centered.
  kAspectFillAlignTop,

  // Fit the image in the target so it fits completely inside, preserving aspect
  // ratio. This will leave bands with with no data in the target.
  // `projectTo` will be set to `originalSize`
  // `revisedTargetSize` will be a smaller in one direction from `targetSize`
  kAspectFit,

  // Scale to the target, maintaining aspect ratio and not clipping the excess.
  // `projectTo` will be set to `originalSize`
  // `revisedTargetSize` will be a larger in one direction from `targetSize`
  kAspectFillNoClipping,
};

void CalculateProjection(CGSize originalSize,
                         CGSize targetSize,
                         ProjectionMode projectionMode,
                         CGSize& revisedTargetSize,
                         CGRect& projectTo);

// Returns an image generated from the given `view`, using `backgroundColor` and
// adding `padding` around the centered image.
UIImage* ImageFromView(UIView* view,
                       UIColor* backgroundColor,
                       UIEdgeInsets padding);

// Returns an image resized to `targetSize`. It first calculate the projection
// by calling CalculateProjection() and then create a new image of the desired
// size and project the correct subset of the original image onto it.
// The resulting image will have an alpha channel.
//
// Image interpolation level for resizing is set to kCGInterpolationDefault.
//
// The resize always preserves the scale of the original image.
UIImage* ResizeImage(UIImage* image,
                     CGSize targetSize,
                     ProjectionMode projectionMode);

// Returns an image resized to `targetSize`. It first calculate the projection
// by calling CalculateProjection() and then create a new image of the desired
// size and project the correct subset of the original image onto it.
// `opaque` determine whether resulting image should have an alpha channel.
// Prefer setting `opaque` to YES for better performances.
//
// Image interpolation level for resizing is set to kCGInterpolationDefault.
//
// The resize always preserves the scale of the original image.
UIImage* ResizeImage(UIImage* image,
                     CGSize targetSize,
                     ProjectionMode projectionMode,
                     BOOL opaque);

// Scale down the image if it's too large so it doesn't take too much space
// to store or too much data to upload.
// This is taken from Google Toolbox for Mac. Extensions should not have
// too many dependencies, so this is reproduced here. This should not be used
// outside of extensions. Instead, use the simpler version in
// "//ui/gfx/image/image_util.h".
UIImage* ResizeImageForSearchByImage(UIImage* image);

// Returns a blurred image generated from the given `image` and `blurRadius`.
UIImage* BlurredImageWithImage(UIImage* image, CGFloat blurRadius);

#endif  // IOS_CHROME_COMMON_UI_UTIL_IMAGE_UTIL_H_
