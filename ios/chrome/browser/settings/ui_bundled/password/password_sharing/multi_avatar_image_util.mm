// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_sharing/multi_avatar_image_util.h"

#import "base/check.h"
#import "base/not_fatal_until.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

namespace {

// Crops `image` to half of its width taking the center part of it, ensuring the
// output is exactly 1:2 aspect ratio.
UIImage* CropToMiddle(UIImage* image) {
  if (!image) {
    return nil;
  }

  CGFloat width = image.size.width;
  CGFloat height = image.size.height;
  CGFloat targetWidth = height / 2.0;

  CGSize targetSize;
  CGRect drawRect;
  if (width <= targetWidth) {
    CGFloat targetHeight = width * 2.0;
    targetSize = CGSizeMake(width, targetHeight);
    drawRect = CGRectMake(0, (targetHeight - height) / 2.0, width, height);
  } else {
    targetSize = CGSizeMake(targetWidth, height);
    drawRect = CGRectMake((targetWidth - width) / 2.0, 0, width, height);
  }

  UIGraphicsImageRendererFormat* format =
      [UIGraphicsImageRendererFormat preferredFormat];
  format.scale = image.scale;
  format.opaque = NO;

  UIGraphicsImageRenderer* renderer =
      [[UIGraphicsImageRenderer alloc] initWithSize:targetSize format:format];
  return
      [renderer imageWithActions:^(
                    UIGraphicsImageRendererContext* _Nonnull rendererContext) {
        [image drawInRect:drawRect];
      }];
}

// Draws a "+N" badge in the bottom-right quadrant, centering the text in the
// sector.
void DrawBadgeInBottomRightCorner(CGRect rect, NSInteger count, CGFloat size) {
  CHECK(UIGraphicsGetCurrentContext(), base::NotFatalUntil::M160);
  [[UIColor colorNamed:kTertiaryBackgroundColor] setFill];
  UIRectFill(rect);

  NSString* text = [NSString stringWithFormat:@"+%ld", (long)count];
  NSDictionary* attributes = @{
    NSFontAttributeName : [UIFont systemFontOfSize:size / 5.0
                                            weight:UIFontWeightBold],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };
  CGSize textSize = [text sizeWithAttributes:attributes];

  // Calculate the visual center of the bottom-right quarter-circle sector.
  // 0.45 * R is slightly outer than the mathematical centroid (0.42 * R)
  // to look visually centered with the text.
  CGFloat R = size / 2;
  CGFloat centerX = R * 1.45;
  CGFloat centerY = R * 1.45;

  CGRect textRect = CGRectMake(centerX - textSize.width / 2.0,
                               centerY - textSize.height / 2.0, textSize.width,
                               textSize.height);
  [text drawInRect:textRect withAttributes:attributes];
}

}  // namespace

UIImage* CreateMultiAvatarImage(NSArray<UIImage*>* images, CGFloat size) {
  NSInteger imagesCount = static_cast<NSInteger>(images.count);
  if (imagesCount == 0) {
    return DefaultSymbolTemplateWithPointSize(kPersonCropCircleSymbol, size);
  }

  if (imagesCount == 1) {
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
  CGRect leftUpperRect = CGRectMake(-kSpacing, -kSpacing, kHalfSize, kHalfSize);
  CGRect leftLowerRect =
      CGRectMake(-kSpacing, kHalfSize + kSpacing, kHalfSize, kHalfSize);
  CGRect rightUpperRect =
      CGRectMake(kHalfSize + kSpacing, -kSpacing, kHalfSize, kHalfSize);
  CGRect rightLowerRect = CGRectMake(kHalfSize + kSpacing, kHalfSize + kSpacing,
                                     kHalfSize, kHalfSize);

  // Define 2 half rectangles.
  CGRect leftRect = CGRectMake(-kSpacing, 0, kHalfSize, size);
  CGRect rightRect = CGRectMake(kHalfSize + kSpacing, 0, kHalfSize, size);

  UIImage* mergedImage =
      [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        // Create the left side of the image.
        if (imagesCount <= 3) {
          [CropToMiddle(images[0]) drawInRect:leftRect];
        } else {
          [images[0] drawInRect:leftUpperRect];
          [images[3] drawInRect:leftLowerRect];
        }

        // Create the right side of the image.
        if (imagesCount == 2) {
          [CropToMiddle(images[1]) drawInRect:rightRect];
        } else {
          [images[1] drawInRect:rightUpperRect];

          if (imagesCount <= 4) {
            [images[2] drawInRect:rightLowerRect];
          } else {
            DrawBadgeInBottomRightCorner(rightLowerRect, imagesCount - 3, size);
          }
        }
      }];

  return CircularImageFromImage(mergedImage, size);
}
