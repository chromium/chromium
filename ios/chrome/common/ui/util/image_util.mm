// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/image_util.h"

#include "ui/gfx/image/resize_image_dimensions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIImage* ResizeImageForSearchByImage(UIImage* image) {
  // Check |image|.
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
  if (targetSize.height < 1 || targetSize.width < 1) {
    return nil;
  }
  CGFloat aspectRatio = imageSize.width / imageSize.height;
  CGFloat targetAspectRatio = targetSize.width / targetSize.height;
  CGRect projectTo = CGRectZero;
  // Scale image to ensure it fits inside the specified targetSize.
  if (targetAspectRatio < aspectRatio) {
    // target is less wide than the original.
    projectTo.size.width = targetSize.width;
    projectTo.size.height = projectTo.size.width / aspectRatio;
    targetSize = projectTo.size;
  } else {
    // target is wider than the original.
    projectTo.size.height = targetSize.height;
    projectTo.size.width = projectTo.size.height * aspectRatio;
    targetSize = projectTo.size;
  }
  projectTo = CGRectIntegral(projectTo);
  // There's no CGSizeIntegral, so we fake our own.
  CGRect integralRect = CGRectZero;
  integralRect.size = targetSize;
  targetSize = CGRectIntegral(integralRect).size;

  // Resize photo. Use UIImage drawing methods because they respect
  // UIImageOrientation as opposed to CGContextDrawImage().
  UIGraphicsBeginImageContext(targetSize);
  [image drawInRect:projectTo];
  UIImage* resizedPhoto = UIGraphicsGetImageFromCurrentImageContext();
  UIGraphicsEndImageContext();
  return resizedPhoto;
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
