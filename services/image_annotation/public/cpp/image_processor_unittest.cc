// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/image_annotation/public/cpp/image_processor.h"

#include <cmath>
#include <limits>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "services/image_annotation/image_annotation_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace image_annotation {

namespace {

using testing::Eq;
using testing::Lt;

constexpr double kMaxError = 1e-6;

// Generates an image of size |dim|x|dim| containing an 8x8 black and white
// checkerboard pattern.
SkBitmap GenCheckerboardBitmap(const int dim) {
  const int check_dim = dim / 8;

  SkBitmap out;
  out.setInfo(SkImageInfo::Make(dim, dim, kRGBA_8888_SkColorType,
                                kUnpremul_SkAlphaType));
  out.allocPixels();

  uint8_t* const pixels = reinterpret_cast<uint8_t*>(out.getPixels());
  for (int row = 0; row < dim; ++row) {
    for (int col = 0; col < dim; ++col) {
      const bool black = ((row / check_dim + col / check_dim) % 2) == 1;
      uint8_t* const byte_pos =
          pixels + row * out.rowBytes() + col * out.bytesPerPixel();

      *reinterpret_cast<uint32_t*>(byte_pos) =
          black ? SK_ColorBLACK : SK_ColorWHITE;
    }
  }

  return out;
}

// Returns the mean sum of squared distance between each channel of each pixel
// in the original and compressed images.
double CalcImageError(const SkBitmap& orig, const SkBitmap& comp) {
  // Only valid to call on images of matching size.
  CHECK(orig.width() == comp.width() && orig.height() == comp.height());

  double sum = 0;
  for (int row = 0; row < orig.width(); ++row) {
    for (int col = 0; col < orig.height(); ++col) {
      const auto orig_col = SkColor4f::FromColor(orig.getColor(col, row));
      const auto comp_col = SkColor4f::FromColor(comp.getColor(col, row));

      for (int i = 0; i < 4; ++i) {
        sum += std::pow(orig_col.vec()[i] - comp_col.vec()[i], 2);
      }
    }
  }

  return sum / (4 * orig.width() * orig.height());
}

// Takes an expected image and the actual image produced, and outputs the
// mean sum of squared distance between their pixels.
void OutputImageError(double* const error,
                      const SkBitmap& expected,
                      const std::vector<uint8_t>& result,
                      const int32_t width,
                      const int32_t height) {
  const std::unique_ptr<SkBitmap> comp =
      gfx::JPEGCodec::Decode(result.data(), result.size());
  CHECK(comp);

  *error = width == expected.width() && height == expected.height()
               ? CalcImageError(expected, *comp)
               : std::numeric_limits<double>::infinity();
}

}  // namespace

TEST(ImageProcessorTest, NullImage) {
  base::test::TaskEnvironment test_task_env;
  base::HistogramTester histogram_tester;

  bool empty_bytes = false;

  // The "get pixels" callback returns a null image, simulating failure to fetch
  // pixels.
  ImageProcessor(base::BindRepeating([]() { return SkBitmap(); }))
      .GetJpgImageData(base::BindOnce(
          [](bool* const empty_bytes, const std::vector<uint8_t>& bytes,
             const int32_t w, const int32_t h) {
            *empty_bytes = bytes.empty() && w == 0 && h == 0;
          },
          &empty_bytes));
  test_task_env.RunUntilIdle();

  EXPECT_THAT(empty_bytes, Eq(true));

  histogram_tester.ExpectUniqueSample(metrics_internal::kSourcePixelCount,
                                      0 /* sample */, 1 /* count */);
}

TEST(ImageProcessorTest, ImageContent) {
  base::test::TaskEnvironment test_task_env;
  base::HistogramTester histogram_tester;

  // Create one image that doesn't need scaling and one image that does.
  const int max_dim = static_cast<int>(std::sqrt(ImageProcessor::kMaxPixels));
  const SkBitmap small_orig = GenCheckerboardBitmap(max_dim);
  const SkBitmap large_orig = GenCheckerboardBitmap(max_dim * 2);

  // Process the image that doesn't need scaling, just to test compression.
  double comp_error = kMaxError;
  ImageProcessor(
      base::BindRepeating([](const SkBitmap& b) { return b; }, small_orig))
      .GetJpgImageData(
          base::BindOnce(&OutputImageError, &comp_error, small_orig));
  test_task_env.RunUntilIdle();
  EXPECT_THAT(comp_error, Lt(kMaxError));

  // Process the image that needs scaling and compression.
  double scale_error = kMaxError;
  ImageProcessor(
      base::BindRepeating([](const SkBitmap& b) { return b; }, large_orig))
      .GetJpgImageData(
          base::BindOnce(&OutputImageError, &scale_error, small_orig));
  test_task_env.RunUntilIdle();
  EXPECT_THAT(scale_error, Lt(kMaxError));

  histogram_tester.ExpectBucketCount(metrics_internal::kSourcePixelCount,
                                     max_dim * max_dim /* sample */,
                                     1 /* count */);
  histogram_tester.ExpectBucketCount(metrics_internal::kSourcePixelCount,
                                     4 * max_dim * max_dim /* sample */,
                                     1 /* count */);
  histogram_tester.ExpectTotalCount(metrics_internal::kSourcePixelCount, 2);
}

}  // namespace image_annotation
