// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_BITMAP_IMAGE_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_BITMAP_IMAGE_METRICS_H_

#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class IntSize;

class PLATFORM_EXPORT BitmapImageMetrics {
  STATIC_ONLY(BitmapImageMetrics);

 public:
  // Values synced with 'DecodedImageType' in
  // src/tools/metrics/histograms/enums.xml. These values are persisted to logs.
  // Entries should not be renumbered and numeric values should never be reused.
  enum DecodedImageType {
    kImageUnknown = 0,
    kImageJPEG = 1,
    kImagePNG = 2,
    kImageGIF = 3,
    kImageWebP = 4,
    kImageICO = 5,
    kImageBMP = 6,
    kDecodedImageTypeEnumEnd = kImageBMP + 1
  };

  // Values synced with 'Gamma' in src/tools/metrics/histograms/enums.xml. These
  // values are persisted to logs. Entries should not be renumbered and numeric
  // values should never be reused.
  enum Gamma {
    kGammaLinear = 0,
    kGammaSRGB = 1,
    kGamma2Dot2 = 2,
    kGammaNonStandard = 3,
    kGammaNull = 4,
    kGammaFail = 5,
    kGammaInvalid = 6,
    kGammaExponent = 7,
    kGammaTable = 8,
    kGammaParametric = 9,
    kGammaNamed = 10,
    kGammaEnd = kGammaNamed + 1,
  };

  // Categories for the JPEG color space histogram. Synced with 'JpegColorSpace'
  // in src/tools/metrics/histograms/enums.xml. These values are persisted to
  // logs. Entries should not be renumbered and numeric values should never be
  // reused.
  enum JpegColorSpace {
    kUnknown = 0,
    kGrayscale = 1,
    kRGB = 2,
    kCMYK = 3,
    kYCCK = 4,
    kYCbCr410 = 5,
    kYCbCr411 = 6,
    kYCbCr420 = 7,
    kYCbCr422 = 8,
    kYCbCr440 = 9,
    kYCbCr444 = 10,
    kYCbCrOther = 11,
    kMaxValue = kYCbCrOther,
  };

  static void CountDecodedImageType(const String& type);
  static void CountImageOrientation(const ImageOrientationEnum);
  // Report the JPEG compression density in 0.01 bits per pixel for an image
  // with a smallest side (width or length) of |image_min_side| and total size
  // in bytes |image_size_bytes|.
  static void CountImageJpegDensity(int image_min_side,
                                    uint64_t density_centi_bpp,
                                    size_t image_size_bytes);
  static void CountJpegArea(const IntSize& size);
  static void CountJpegColorSpace(JpegColorSpace color_space);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_BITMAP_IMAGE_METRICS_H_
