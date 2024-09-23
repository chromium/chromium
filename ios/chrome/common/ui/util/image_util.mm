// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <CoreImage/CoreImage.h>

#import "ios/chrome/common/ui/util/image_util.h"

#import "ui/gfx/image/resize_image_dimensions.h"

UIImage* ResizeImage(UIImage* image,
                     CGSize targetSize,
                     ProjectionMode projectionMode) {
  return ResizeImage(image, targetSize, projectionMode, NO);
}

UIImage* ResizeImage(UIImage* image,
                     CGSize targetSize,
                     ProjectionMode projectionMode,
                     BOOL opaque) {
  CGSize revisedTargetSize;
  CGRect projectTo;

  CalculateProjection([image size], targetSize, projectionMode,
                      revisedTargetSize, projectTo);

  if (CGRectEqualToRect(projectTo, CGRectZero))
    return nil;

  // Resize photo. Use UIImage drawing methods because they respect
  // UIImageOrientation as opposed to CGContextDrawImage().
  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.opaque = opaque;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:revisedTargetSize
                                             format:format];

  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    [image drawInRect:projectTo];
  }];
}

UIImage* ResizeImageForSearchByImage(UIImage* image) {
  // Check `image`.
  if (!image) {
    return nil;
  }
  CGSize imageSize = [image size];
  if (imageSize.height < 1 || imageSize.width < 1) {
    return nil;
  }

  // Image is already smaller than the max allowed area.
  if (image.size.height * image.size.width < gfx::kSearchByImageMaxImageArea) {
    return image;
  }
  // If one dimension is small enough, then no need to resize.
  if ((image.size.width < gfx::kSearchByImageMaxImageWidth &&
       image.size.height < gfx::kSearchByImageMaxImageHeight)) {
    return image;
  }

  CGSize targetSize = CGSizeMake(gfx::kSearchByImageMaxImageWidth,
                                 gfx::kSearchByImageMaxImageHeight);
  return ResizeImage(image, targetSize, ProjectionMode::kAspectFit);
}

UIImage* ImageFromView(UIView* view,
                       UIColor* backgroundColor,
                       UIEdgeInsets padding) {
  // Overall bounds of the generated image.
  CGRect imageBounds = CGRectMake(
      0.0, 0.0, view.bounds.size.width + padding.left + padding.right,
      view.bounds.size.height + padding.top + padding.bottom);

  // Centered bounds for drawing the view's content.
  CGRect contentBounds =
      CGRectMake(padding.left, padding.top, view.bounds.size.width,
                 view.bounds.size.height);

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithBounds:imageBounds];
  return [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
    // Draw background.
    [backgroundColor set];
    UIRectFill(imageBounds);

    // Draw view.
    [view drawViewHierarchyInRect:contentBounds afterScreenUpdates:YES];
  }];
}

// Based on an original size and a target size applies the transformations.
void CalculateProjection(CGSize originalSize,
                         CGSize desiredTargetSize,
                         ProjectionMode projectionMode,
                         CGSize& targetSize,
                         CGRect& projectTo) {
  targetSize = desiredTargetSize;
  projectTo = CGRectZero;
  if (originalSize.height < 1 || originalSize.width < 1)
    return;
  if (targetSize.height < 1 || targetSize.width < 1)
    return;

  CGFloat aspectRatio = originalSize.width / originalSize.height;
  CGFloat targetAspectRatio = targetSize.width / targetSize.height;
  switch (projectionMode) {
    case ProjectionMode::kFill:
      // Don't preserve the aspect ratio.
      projectTo.size = targetSize;
      break;

    case ProjectionMode::kAspectFill:
    case ProjectionMode::kAspectFillAlignTop:
      if (targetAspectRatio < aspectRatio) {
        // Clip the x-axis.
        projectTo.size.width = targetSize.height * aspectRatio;
        projectTo.size.height = targetSize.height;
        projectTo.origin.x = (targetSize.width - projectTo.size.width) / 2;
        projectTo.origin.y = 0;
      } else {
        // Clip the y-axis.
        projectTo.size.width = targetSize.width;
        projectTo.size.height = targetSize.width / aspectRatio;
        projectTo.origin.x = 0;
        projectTo.origin.y = (targetSize.height - projectTo.size.height) / 2;
      }
      if (projectionMode == ProjectionMode::kAspectFillAlignTop) {
        projectTo.origin.y = 0;
      }
      break;

    case ProjectionMode::kAspectFit:
      if (targetAspectRatio < aspectRatio) {
        projectTo.size.width = targetSize.width;
        projectTo.size.height = projectTo.size.width / aspectRatio;
        targetSize = projectTo.size;
      } else {
        projectTo.size.height = targetSize.height;
        projectTo.size.width = projectTo.size.height * aspectRatio;
        targetSize = projectTo.size;
      }
      break;

    case ProjectionMode::kAspectFillNoClipping:
      if (targetAspectRatio < aspectRatio) {
        targetSize.width = targetSize.height * aspectRatio;
        targetSize.height = targetSize.height;
      } else {
        targetSize.width = targetSize.width;
        targetSize.height = targetSize.width / aspectRatio;
      }
      projectTo.size = targetSize;
      break;
  }

  projectTo = CGRectIntegral(projectTo);
  // There's no CGSizeIntegral, faking one instead.
  CGRect integralRect = CGRectZero;
  integralRect.size = targetSize;
  targetSize = CGRectIntegral(integralRect).size;
}

UIImage* BlurredImageWithImage(UIImage* image, CGFloat blurRadius) {
  CIImage* inputImage = [CIImage imageWithCGImage:image.CGImage];

  // Extend the edges with a Affline Clamp filter.
  CIFilter* clampFilter = [CIFilter filterWithName:@"CIAffineClamp"];
  [clampFilter setDefaults];
  [clampFilter setValue:inputImage forKey:kCIInputImageKey];

  // Blur the UIImage with a Gaussian blur filter.
  CIFilter* blurFilter = [CIFilter filterWithName:@"CIGaussianBlur"];
  [blurFilter setValue:clampFilter.outputImage forKey:kCIInputImageKey];
  [blurFilter setValue:[NSNumber numberWithFloat:blurRadius]
                forKey:@"inputRadius"];

  CIContext* context = [CIContext contextWithOptions:nil];
  UIImage* blurredImage =
      [UIImage imageWithCGImage:[context createCGImage:blurFilter.outputImage
                                              fromRect:inputImage.extent]];
  return blurredImage;
}
