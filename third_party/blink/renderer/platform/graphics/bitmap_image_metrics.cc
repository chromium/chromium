// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/bitmap_image_metrics.h"

#include "base/metrics/histogram_base.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/color_space_gamut.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/skia/third_party/skcms/skcms.h"

namespace blink {

void BitmapImageMetrics::CountDecodedImageType(const String& type) {
  DecodedImageType decoded_image_type =
      type == "jpg"
          ? kImageJPEG
          : type == "png"
                ? kImagePNG
                : type == "gif"
                      ? kImageGIF
                      : type == "webp"
                            ? kImageWebP
                            : type == "ico"
                                  ? kImageICO
                                  : type == "bmp"
                                        ? kImageBMP
                                        : DecodedImageType::kImageUnknown;

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, decoded_image_type_histogram,
      ("Blink.DecodedImageType", kDecodedImageTypeEnumEnd));
  decoded_image_type_histogram.Count(decoded_image_type);
}

void BitmapImageMetrics::CountImageOrientation(
    const ImageOrientationEnum orientation) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, orientation_histogram,
      ("Blink.DecodedImage.Orientation", kImageOrientationEnumEnd));
  orientation_histogram.Count(orientation);
}

void BitmapImageMetrics::CountImageJpegDensity(int image_min_side,
                                               int64_t density_centi_bpp) {
  // Values are reported in the range 0.01 to 10 bpp, in different metrics
  // depending on the image category (small, medium, large).
  if (image_min_side >= 1000) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, density_histogram,
        ("Blink.DecodedImage.JpegDensity.1000px", 1, 1000, 100));
    density_histogram.Count(density_centi_bpp);
  } else if (image_min_side >= 400) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, density_histogram,
        ("Blink.DecodedImage.JpegDensity.400px", 1, 1000, 100));
    density_histogram.Count(density_centi_bpp);
  } else if (image_min_side >= 100) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, density_histogram,
        ("Blink.DecodedImage.JpegDensity.100px", 1, 1000, 100));
    density_histogram.Count(density_centi_bpp);
  } else {
    // We don't report for images with 0 to 99px on the smallest dimension.
  }
}

void BitmapImageMetrics::CountImageGammaAndGamut(
    const skcms_ICCProfile* color_profile) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(EnumerationHistogram, gamma_named_histogram,
                                  ("Blink.ColorSpace.Source", kGammaEnd));
  gamma_named_histogram.Count(GetColorSpaceGamma(color_profile));

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, gamut_named_histogram,
      ("Blink.ColorGamut.Source", static_cast<int>(ColorSpaceGamut::kEnd)));
  gamut_named_histogram.Count(static_cast<int>(
      color_space_utilities::GetColorSpaceGamut(color_profile)));
}

void BitmapImageMetrics::CountJpegArea(const IntSize& size) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, image_area_histogram,
      ("Blink.ImageDecoders.Jpeg.Area", 1 /* min */, 8192 * 8192 /* max */,
       100 /* bucket_count */));
  // A base::HistogramBase::Sample may not fit |size.Area()|. Hence the use of
  // saturated_cast.
  image_area_histogram.Count(
      base::saturated_cast<base::HistogramBase::Sample>(size.Area()));
}

void BitmapImageMetrics::CountJpegColorSpace(JpegColorSpace color_space) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      EnumerationHistogram, color_space_histogram,
      ("Blink.ImageDecoders.Jpeg.ColorSpace", JpegColorSpace::kMaxValue));
  color_space_histogram.Count(color_space);
}

BitmapImageMetrics::Gamma BitmapImageMetrics::GetColorSpaceGamma(
    const skcms_ICCProfile* color_profile) {
  Gamma gamma = kGammaNull;
  if (color_profile) {
    if (skcms_TRCs_AreApproximateInverse(
            color_profile, skcms_sRGB_Inverse_TransferFunction())) {
      gamma = kGammaSRGB;
    } else if (skcms_TRCs_AreApproximateInverse(
                   color_profile, skcms_Identity_TransferFunction())) {
      gamma = kGammaLinear;
    } else {
      gamma = kGammaNonStandard;
    }
  }
  return gamma;
}

}  // namespace blink
