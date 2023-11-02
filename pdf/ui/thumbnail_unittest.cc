// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ui/thumbnail.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace chrome_pdf {

namespace {

constexpr float kDeviceToPixelLow = 1;
constexpr float kDeviceToPixelHigh = 2;

struct BestFitSizeParams {
  float device_pixel_ratio;
  gfx::Size page_size;
  gfx::Size expected_thumbnail_size;
};

void TestBestFitSize(const BestFitSizeParams& params) {
  Thumbnail thumbnail(params.page_size, params.device_pixel_ratio);
  EXPECT_EQ(thumbnail.image_size(), params.expected_thumbnail_size)
      << "Failed for page of size of " << params.page_size.ToString()
      << " and device to pixel ratio of " << params.device_pixel_ratio;
}

TEST(ThumbnailTest, CalculateBestFitSizeNormal) {
  static constexpr BestFitSizeParams kBestFitSizeTestParams[] = {
      {kDeviceToPixelLow, {612, 792}, {108, 140}},    // ANSI Letter
      {kDeviceToPixelLow, {595, 842}, {108, 152}},    // ISO 216 A4
      {kDeviceToPixelLow, {200, 200}, {140, 140}},    // Square
      {kDeviceToPixelLow, {1000, 200}, {540, 108}},   // Wide
      {kDeviceToPixelLow, {200, 1000}, {108, 540}},   // Tall
      {kDeviceToPixelLow, {1500, 50}, {1399, 46}},    // Super wide
      {kDeviceToPixelLow, {50, 1500}, {46, 1399}},    // Super tall
      {kDeviceToPixelHigh, {612, 792}, {216, 280}},   // ANSI Letter
      {kDeviceToPixelHigh, {595, 842}, {214, 303}},   // ISO 216 A4
      {kDeviceToPixelHigh, {200, 200}, {255, 255}},   // Square
      {kDeviceToPixelHigh, {1000, 200}, {571, 114}},  // Wide
      {kDeviceToPixelHigh, {200, 1000}, {114, 571}},  // Tall
      {kDeviceToPixelHigh, {1500, 50}, {1399, 46}},   // Super wide
      {kDeviceToPixelHigh, {50, 1500}, {46, 1399}},   // Super tall
  };

  for (const auto& params : kBestFitSizeTestParams)
    TestBestFitSize(params);
}

TEST(ThumbnailTest, CalculateBestFitSizeLargeAspectRatio) {
  static constexpr BestFitSizeParams kBestFitSizeTestParams[] = {
      {kDeviceToPixelLow, {14400, 3}, {17701, 3}},     // PDF 1.7 widest
      {kDeviceToPixelLow, {3, 14400}, {3, 17701}},     // PDF 1.7 tallest
      {kDeviceToPixelLow, {0, 0}, {140, 140}},         // Empty
      {kDeviceToPixelLow, {9999999, 1}, {17701, 3}},   // Very wide
      {kDeviceToPixelLow, {1, 9999999}, {3, 17701}},   // Very tall
      {kDeviceToPixelHigh, {14400, 3}, {17701, 3}},    // PDF 1.7 widest
      {kDeviceToPixelHigh, {3, 14400}, {3, 17701}},    // PDF 1.7 tallest
      {kDeviceToPixelHigh, {0, 0}, {255, 255}},        // Empty
      {kDeviceToPixelHigh, {9999999, 1}, {17701, 3}},  // Very wide
      {kDeviceToPixelHigh, {1, 9999999}, {3, 17701}},  // Very tall
  };

  for (const auto& params : kBestFitSizeTestParams)
    TestBestFitSize(params);
}

TEST(ThumbnailTest, CalculateBestFitSizeNoOverflow) {
  static constexpr BestFitSizeParams kBestFitSizeTestParams[] = {
      {kDeviceToPixelLow, {9999999, 9999999}, {140, 140}},   // Very large
      {kDeviceToPixelHigh, {9999999, 9999999}, {255, 255}},  // Very large
  };

  for (const auto& params : kBestFitSizeTestParams)
    TestBestFitSize(params);
}

}  // namespace

}  // namespace chrome_pdf
