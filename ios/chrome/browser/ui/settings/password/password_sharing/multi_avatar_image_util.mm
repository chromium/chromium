// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/multi_avatar_image_util.h"

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

namespace {

// Crops `image` to half of its width taking the center part of it.
UIImage* CropToMiddle(UIImage* image) {
  // Skip cropping if image is already portrait oriented.
  // TODO(crbug.com/40275395): Consider adding a CHECK or different handling of
  // this case.
  if (image.size.width < image.size.height) {
    return image;
  }

  CGRect cropRect = CGRectMake(image.size.width / 4, 0, image.size.width / 2,
                               image.size.height);
  CGImageRef imageRef = CGImageCreateWithImageInRect([image CGImage], cropRect);
  UIImage* newImage = [UIImage imageWithCGImage:imageRef];
  CGImageRelease(imageRef);
  return newImage;
}

}  // namespace

UIImage* CreateMultiAvatarImage(NSArray<UIImage*>* images, CGFloat size) {
  if (images.count == 0) {
    return DefaultSymbolTemplateWithPointSize(kPersonCropCircleSymbol, size);
  }

  if (images.count == 1) {
    return CircularImageFromImage(images[0], size);
  }

  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.opaque = NO;
  CGRect rect = CGRectMake(0, 0, size, size);
  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:rect.size format:format];

  // The images should be spaced from the middle towards their quarter / half.
  CGFloat kSpacing = 1.0;
  CGFloat kHalfSize = size / 2;

  // Define 4 quarter rectangles.
  CGRect leftUpperRect =
      CGRectMake(-kSpacing, kHalfSize + kSpacing, kHalfSize, kHalfSize);
  CGRect rightUpperRect = CGRectMake(kHalfSize + kSpacing, kHalfSize + kSpacing,
                                     kHalfSize, kHalfSize);
  CGRect rightLowerRect =
      CGRectMake(kHalfSize + kSpacing, -kSpacing, kHalfSize, kHalfSize);
  CGRect leftLowerRect = CGRectMake(-kSpacing, -kSpacing, kHalfSize, kHalfSize);

  // Define 2 half rectangles.
  CGRect leftRect = CGRectMake(-kSpacing, 0, kHalfSize, size);
  CGRect rightRect = CGRectMake(kHalfSize + kSpacing, 0, kHalfSize, size);

  UIImage* mergedImage =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        // Create the left side of the image.
        if (images.count <= 3) {
          [CropToMiddle(images[0]) drawInRect:leftRect];
        } else {
          [images[0] drawInRect:leftUpperRect];
          [images[3] drawInRect:leftLowerRect];
        }

        // Create the right side of the image.
        // TODO(crbug.com/40275395): Handle the case of more than 4 images.
        if (images.count == 2) {
          [CropToMiddle(images[1]) drawInRect:rightRect];
        } else {
          [images[1] drawInRect:rightUpperRect];
          [images[2] drawInRect:rightLowerRect];
        }
      }];

  return CircularImageFromImage(mergedImage, size);
}
