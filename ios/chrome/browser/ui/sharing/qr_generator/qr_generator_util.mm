// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/qr_generator/qr_generator_util.h"

#import <CoreImage/CoreImage.h>

UIImage* GenerateQRCode(NSData* data, CGFloat imageLength) {
  // Generate the QR Code with the given data.
  CIFilter* qrFilter = [CIFilter filterWithName:@"CIQRCodeGenerator"];
  [qrFilter setValue:data forKey:@"inputMessage"];
  [qrFilter setValue:@"L" forKey:@"inputCorrectionLevel"];
  CIImage* ciImage = qrFilter.outputImage;

  // Scale the square image.
  imageLength *= [[UIScreen mainScreen] scale];
  CGFloat scale = imageLength / ciImage.extent.size.width;
  ciImage = [ciImage
      imageByApplyingTransform:CGAffineTransformMakeScale(scale, scale)];

  return [UIImage imageWithCIImage:ciImage
                             scale:[[UIScreen mainScreen] scale]
                       orientation:UIImageOrientationUp];
}
