// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "skia/ext/image_operations.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace {

// Computes the average pixel value for the given range, inclusive.
uint32_t AveragePixel(const SkBitmap& bmp,
                      int x_min, int x_max,
                      int y_min, int y_max) {
  float accum[4] = {0, 0, 0, 0};
  int count = 0;
  for (int y = y_min; y <= y_max; y++) {
    for (int x = x_min; x <= x_max; x++) {
      uint32_t cur = *bmp.getAddr32(x, y);
      accum[0] += SkColorGetB(cur);
      accum[1] += SkColorGetG(cur);
      accum[2] += SkColorGetR(cur);
      accum[3] += SkColorGetA(cur);
      count++;
    }
  }

  return SkColorSetARGB(static_cast<unsigned char>(accum[3] / count),
                        static_cast<unsigned char>(accum[2] / count),
                        static_cast<unsigned char>(accum[1] / count),
                        static_cast<unsigned char>(accum[0] / count));
}

// Computes the average pixel (/color) value for the given colors.
SkColor AveragePixel(const SkColor colors[], size_t color_count) {
  float accum[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
  for (size_t i = 0; i < color_count; ++i) {
    const SkColor cur = colors[i];
    accum[0] += static_cast<float>(SkColorGetA(cur));
    accum[1] += static_cast<float>(SkColorGetR(cur));
    accum[2] += static_cast<float>(SkColorGetG(cur));
    accum[3] += static_cast<float>(SkColorGetB(cur));
  }
  const SkColor average_color =
      SkColorSetARGB(static_cast<uint8_t>(accum[0] / color_count),
                     static_cast<uint8_t>(accum[1] / color_count),
                     static_cast<uint8_t>(accum[2] / color_count),
                     static_cast<uint8_t>(accum[3] / color_count));
  return average_color;
}

void PrintPixel(const SkBitmap& bmp,
                int x_min, int x_max,
                int y_min, int y_max) {
  char str[128];

  for (int y = y_min; y <= y_max; ++y) {
    for (int x = x_min; x <= x_max; ++x) {
      const uint32_t cur = *bmp.getAddr32(x, y);
      base::snprintf(str, sizeof(str), "bmp[%d,%d] = %08X", x, y, cur);
      ADD_FAILURE() << str;
    }
  }
}

// Returns the euclidian distance between two RGBA colors interpreted
// as 4-components vectors.
//
// Notes:
// - This is a really poor definition of color distance. Yet it
//   is "good enough" for our uses here.
// - More realistic measures like the various Delta E formulas defined
//   by CIE are way more complex and themselves require the RGBA to
//   to transformed into CIELAB (typically via sRGB first).
// - The static_cast<int> below are needed to avoid interpreting "negative"
//   differences as huge positive values.
float ColorsEuclidianDistance(const SkColor a, const SkColor b) {
  int b_int_diff = static_cast<int>(SkColorGetB(a) - SkColorGetB(b));
  int g_int_diff = static_cast<int>(SkColorGetG(a) - SkColorGetG(b));
  int r_int_diff = static_cast<int>(SkColorGetR(a) - SkColorGetR(b));
  int a_int_diff = static_cast<int>(SkColorGetA(a) - SkColorGetA(b));

  float b_float_diff = static_cast<float>(b_int_diff);
  float g_float_diff = static_cast<float>(g_int_diff);
  float r_float_diff = static_cast<float>(r_int_diff);
  float a_float_diff = static_cast<float>(a_int_diff);

  return sqrtf((b_float_diff * b_float_diff) + (g_float_diff * g_float_diff) +
               (r_float_diff * r_float_diff) + (a_float_diff * a_float_diff));
}

// Returns true if each channel of the given two colors are "close." This is
// used for comparing colors where rounding errors may cause off-by-one.
bool ColorsClose(uint32_t a, uint32_t b) {
  return abs(static_cast<int>(SkColorGetB(a) - SkColorGetB(b))) < 2 &&
         abs(static_cast<int>(SkColorGetG(a) - SkColorGetG(b))) < 2 &&
         abs(static_cast<int>(SkColorGetR(a) - SkColorGetR(b))) < 2 &&
         abs(static_cast<int>(SkColorGetA(a) - SkColorGetA(b))) < 2;
}

void FillDataToBitmap(int w, int h, SkBitmap* bmp) {
  bmp->allocPixels(SkImageInfo::MakeN32Premul(w, h, SkColorSpace::MakeSRGB()));

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const uint8_t component = static_cast<uint8_t>(y * w + x);
      const SkColor pixel = SkColorSetARGB(component, component,
                                           component, component);
      *bmp->getAddr32(x, y) = pixel;
    }
  }
}

// Draws a checkerboard pattern into the w x h bitmap passed in.
// Each rectangle is rect_w in width, rect_h in height.
// The colors alternate between color1 and color2, color1 being used
// in the rectangle at the top left corner.
void DrawCheckerToBitmap(int w, int h,
                         SkColor color1, SkColor color2,
                         int rect_w, int rect_h,
                         SkBitmap* bmp) {
  ASSERT_GT(rect_w, 0);
  ASSERT_GT(rect_h, 0);
  ASSERT_NE(color1, color2);

  bmp->allocPixels(SkImageInfo::MakeN32Premul(w, h, SkColorSpace::MakeSRGB()));

  for (int y = 0; y < h; ++y) {
    bool y_bit = (((y / rect_h) & 0x1) == 0);

    for (int x = 0; x < w; ++x) {
      bool x_bit = (((x / rect_w) & 0x1) == 0);

      bool use_color2 = (x_bit != y_bit);  // xor

      *bmp->getAddr32(x, y) = (use_color2 ? color2 : color1);
    }
  }
}

// DEBUG_BITMAP_GENERATION (0 or 1) controls whether the routines
// to save the test bitmaps are present. By default the test just fails
// without reading/writing files but it is then convenient to have
// a simple way to make the failing tests write out the input/output images
// to check them visually.
#define DEBUG_BITMAP_GENERATION (0)

#if DEBUG_BITMAP_GENERATION
void SaveBitmapToPNG(const SkBitmap& bmp, const char* path) {
  std::vector<unsigned char> png;
  gfx::PNGCodec::ColorFormat color_format = gfx::PNGCodec::FORMAT_RGBA;
  if (!gfx::PNGCodec::Encode(
          reinterpret_cast<const unsigned char*>(bmp.getPixels()),
          color_format, gfx::Size(bmp.width(), bmp.height()),
          static_cast<int>(bmp.rowBytes()),
          false, std::vector<gfx::PNGCodec::Comment>(), &png)) {
    FAIL() << "Failed to encode image";
  }

  const base::FilePath fpath(path);
  if (!base::WriteFile(fpath, png)) {
    FAIL() << "Failed to write dest \"" << path << '"';
  }
}
#endif  // #if DEBUG_BITMAP_GENERATION

void CheckResampleToSame(skia::ImageOperations::ResizeMethod method) {
  // Make our source bitmap.
  const int src_w = 16, src_h = 34;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  // Do a resize of the full bitmap to the same size. The lanczos filter is good
  // enough that we should get exactly the same image for output.
  SkBitmap results = skia::ImageOperations::Resize(src, method, src_w, src_h);
  ASSERT_EQ(src_w, results.width());
  ASSERT_EQ(src_h, results.height());
  EXPECT_TRUE(results.colorSpace() && results.colorSpace()->isSRGB());

  for (int y = 0; y < src_h; y++) {
    for (int x = 0; x < src_w; x++) {
      EXPECT_EQ(*src.getAddr32(x, y), *results.getAddr32(x, y));
    }
  }
}

// Types defined outside of the ResizeShouldAverageColors test to allow
// use of the std::size() macro.
//
// 'max_color_distance_override' is used in a max() call together with
// the value of 'max_color_distance' defined in a TestedPixel instance.
// Hence a value of 0.0 in 'max_color_distance_override' means
// "use the pixel-specific value" and larger values can be used to allow
// worse computation errors than provided in a TestedPixel instance.
struct TestedResizeMethod {
  skia::ImageOperations::ResizeMethod method;
  const char* name;
  float max_color_distance_override;
};

struct TestedPixel {
  int         x;
  int         y;
  float       max_color_distance;
  const char* name;
};

// Helper function used by the test "ResizeShouldAverageColors" below.
// Note that ASSERT_EQ does a "return;" on failure, hence we can't have
// a "bool" return value to reflect success. Hence "all_pixels_pass"
void CheckResizeMethodShouldAverageGrid(
    const SkBitmap& src,
    const TestedResizeMethod& tested_method,
    int dest_w, int dest_h, SkColor average_color,
    bool* method_passed) {
  *method_passed = false;

  const TestedPixel tested_pixels[] = {
    // Corners
    { 0,          0,           2.3f, "Top left corner"  },
    { 0,          dest_h - 1,  2.3f, "Bottom left corner" },
    { dest_w - 1, 0,           2.3f, "Top right corner" },
    { dest_w - 1, dest_h - 1,  2.3f, "Bottom right corner" },
    // Middle points of each side
    { dest_w / 2, 0,           1.0f, "Top middle" },
    { dest_w / 2, dest_h - 1,  1.0f, "Bottom middle" },
    { 0,          dest_h / 2,  1.0f, "Left middle" },
    { dest_w - 1, dest_h / 2,  1.0f, "Right middle" },
    // Center
    { dest_w / 2, dest_h / 2,  1.0f, "Center" }
  };

  // Resize the src
  const skia::ImageOperations::ResizeMethod method = tested_method.method;

  SkBitmap dest = skia::ImageOperations::Resize(src, method, dest_w, dest_h);
  ASSERT_EQ(dest_w, dest.width());
  ASSERT_EQ(dest_h, dest.height());
  EXPECT_TRUE(dest.colorSpace() && dest.colorSpace()->isSRGB());

  // Check that pixels match the expected average.
  float max_observed_distance = 0.0f;
  bool all_pixels_ok = true;

  for (size_t pixel_index = 0; pixel_index < std::size(tested_pixels);
       ++pixel_index) {
    const TestedPixel& tested_pixel = tested_pixels[pixel_index];

    const int   x = tested_pixel.x;
    const int   y = tested_pixel.y;
    const float max_allowed_distance =
        std::max(tested_pixel.max_color_distance,
                 tested_method.max_color_distance_override);

    const SkColor actual_color = *dest.getAddr32(x, y);

    // Check that the pixels away from the border region are very close
    // to the expected average color
    float distance = ColorsEuclidianDistance(average_color, actual_color);

    EXPECT_LE(distance, max_allowed_distance)
        << "Resizing method: " << tested_method.name
        << ", pixel tested: " << tested_pixel.name
        << "(" << x << ", " << y << ")"
        << std::hex << std::showbase
        << ", expected (avg) hex: " <<  average_color
        << ", actual hex: " << actual_color;

    if (distance > max_allowed_distance) {
      all_pixels_ok = false;
    }
    if (distance > max_observed_distance) {
      max_observed_distance = distance;
    }
  }

  if (!all_pixels_ok) {
    ADD_FAILURE() << "Maximum observed color distance for method "
                  << tested_method.name << ": " << max_observed_distance;

#if DEBUG_BITMAP_GENERATION
    char path[128];
    base::snprintf(path, sizeof(path),
                   "/tmp/ResizeShouldAverageColors_%s_dest.png",
                   tested_method.name);
    SaveBitmapToPNG(dest, path);
#endif  // #if DEBUG_BITMAP_GENERATION
  }

  *method_passed = all_pixels_ok;
}


}  // namespace

// Helper tests that saves bitmaps to PNGs in /tmp/ to visually check
// that the bitmap generation functions work as expected.
// Those tests are not enabled by default as verification is done
// manually/visually, however it is convenient to leave the functions
// in place.
#if 0 && DEBUG_BITMAP_GENERATION
TEST(ImageOperations, GenerateGradientBitmap) {
  // Make our source bitmap.
  const int src_w = 640, src_h = 480;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  SaveBitmapToPNG(src, "/tmp/gradient_640x480.png");
}

TEST(ImageOperations, GenerateGridBitmap) {
  const int src_w = 640, src_h = 480, src_grid_pitch = 10, src_grid_width = 4;
  const SkColor grid_color = SK_ColorRED, background_color = SK_ColorBLUE;
  SkBitmap src;
  DrawGridToBitmap(src_w, src_h,
                   background_color, grid_color,
                   src_grid_pitch, src_grid_width,
                   &src);

  SaveBitmapToPNG(src, "/tmp/grid_640x408_10_4_red_blue.png");
}

TEST(ImageOperations, GenerateCheckerBitmap) {
  const int src_w = 640, src_h = 480, rect_w = 10, rect_h = 4;
  const SkColor color1 = SK_ColorRED, color2 = SK_ColorBLUE;
  SkBitmap src;
  DrawCheckerToBitmap(src_w, src_h, color1, color2, rect_w, rect_h, &src);

  SaveBitmapToPNG(src, "/tmp/checker_640x408_10_4_red_blue.png");
}
#endif  // #if ... && DEBUG_BITMAP_GENERATION

// Makes the bitmap 50% the size as the original using a box filter. This is
// an easy operation that we can check the results for manually.
TEST(ImageOperations, Halve) {
  // Make our source bitmap.
  int src_w = 30, src_h = 38;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  // Do a halving of the full bitmap.
  SkBitmap actual_results = skia::ImageOperations::Resize(
      src, skia::ImageOperations::RESIZE_BOX, src_w / 2, src_h / 2);
  ASSERT_EQ(src_w / 2, actual_results.width());
  ASSERT_EQ(src_h / 2, actual_results.height());
  EXPECT_TRUE(actual_results.colorSpace() &&
              actual_results.colorSpace()->isSRGB());

  // Compute the expected values & compare.
  for (int y = 0; y < actual_results.height(); y++) {
    for (int x = 0; x < actual_results.width(); x++) {
      // Note that those expressions take into account the "half-pixel"
      // offset that comes into play due to considering the coordinates
      // of the center of the pixels. So x * 2 is a simplification
      // of ((x+0.5) * 2 - 1) and (x * 2 + 1) is really (x + 0.5) * 2.
      int first_x = x * 2;
      int last_x = std::min(src_w - 1, x * 2 + 1);

      int first_y = y * 2;
      int last_y = std::min(src_h - 1, y * 2 + 1);

      const uint32_t expected_color = AveragePixel(src,
                                                   first_x, last_x,
                                                   first_y, last_y);
      const uint32_t actual_color = *actual_results.getAddr32(x, y);
      const bool close = ColorsClose(expected_color, actual_color);
      EXPECT_TRUE(close);
      if (!close) {
        char str[128];
        base::snprintf(str, sizeof(str),
                       "exp[%d,%d] = %08X, actual[%d,%d] = %08X",
                       x, y, expected_color, x, y, actual_color);
        ADD_FAILURE() << str;
        PrintPixel(src, first_x, last_x, first_y, last_y);
      }
    }
  }
}

TEST(ImageOperations, HalveSubset) {
  // Make our source bitmap.
  int src_w = 16, src_h = 34;
  SkBitmap src;
  FillDataToBitmap(src_w, src_h, &src);

  // Do a halving of the full bitmap.
  SkBitmap full_results = skia::ImageOperations::Resize(
      src, skia::ImageOperations::RESIZE_BOX, src_w / 2, src_h / 2);
  ASSERT_EQ(src_w / 2, full_results.width());
  ASSERT_EQ(src_h / 2, full_results.height());
  EXPECT_TRUE(full_results.colorSpace() && full_results.colorSpace()->isSRGB());

  // Now do a halving of a a subset, recall the destination subset is in the
  // destination coordinate system (max = half of the original image size).
  SkIRect subset_rect = { 2, 3, 3, 6 };
  SkBitmap subset_results = skia::ImageOperations::Resize(
      src, skia::ImageOperations::RESIZE_BOX,
      src_w / 2, src_h / 2, subset_rect);
  ASSERT_EQ(subset_rect.width(), subset_results.width());
  ASSERT_EQ(subset_rect.height(), subset_results.height());
  EXPECT_TRUE(subset_results.colorSpace() &&
              subset_results.colorSpace()->isSRGB());

  // The computed subset and the corresponding subset of the original image
  // should be the same.
  for (int y = 0; y < subset_rect.height(); y++) {
    for (int x = 0; x < subset_rect.width(); x++) {
      ASSERT_EQ(
          *full_results.getAddr32(x + subset_rect.fLeft, y + subset_rect.fTop),
          *subset_results.getAddr32(x, y));
    }
  }
}

TEST(ImageOperations, InvalidParams) {
  // Make our source bitmap.
  SkBitmap src;
  src.allocPixels(SkImageInfo::MakeA8(16, 34));

  // Scale it, don't die.
  SkBitmap full_results = skia::ImageOperations::Resize(
      src, skia::ImageOperations::RESIZE_BOX, 10, 20);
}

// Resamples an image to the same image, it should give the same result.
TEST(ImageOperations, ResampleToSameHamming1) {
  CheckResampleToSame(skia::ImageOperations::RESIZE_HAMMING1);
}

TEST(ImageOperations, ResampleToSameLanczos3) {
  CheckResampleToSame(skia::ImageOperations::RESIZE_LANCZOS3);
}

// Check that all Good/Better/Best, Box, Lanczos2 and Lanczos3 generate purple
// when resizing a 4x8 red/blue checker pattern by 1/16x1/16.
TEST(ImageOperations, ResizeShouldAverageColors) {
  // Make our source bitmap.
  const int src_w = 640, src_h = 480, checker_rect_w = 4, checker_rect_h = 8;
  const SkColor checker_color1 = SK_ColorRED, checker_color2 = SK_ColorBLUE;

  const int dest_w = src_w / (4 * checker_rect_w);
  const int dest_h = src_h / (2 * checker_rect_h);

  // Compute the expected (average) color
  const SkColor colors[] = { checker_color1, checker_color2 };
  const SkColor average_color = AveragePixel(colors, std::size(colors));

  static const TestedResizeMethod tested_methods[] = {
    { skia::ImageOperations::RESIZE_GOOD,     "GOOD",     0.0f },
    { skia::ImageOperations::RESIZE_BETTER,   "BETTER",   0.0f },
    { skia::ImageOperations::RESIZE_BEST,     "BEST",     0.0f },
    { skia::ImageOperations::RESIZE_BOX,      "BOX",      0.0f },
    { skia::ImageOperations::RESIZE_HAMMING1, "HAMMING1", 0.0f },
    { skia::ImageOperations::RESIZE_LANCZOS3, "LANCZOS3", 0.0f },
  };

  // Create our source bitmap.
  SkBitmap src;
  DrawCheckerToBitmap(src_w, src_h,
                      checker_color1, checker_color2,
                      checker_rect_w, checker_rect_h,
                      &src);

  // For each method, downscale by 16 in each dimension,
  // and check each tested pixel against the expected average color.
  bool all_methods_ok = true;

  for (size_t method_index = 0; method_index < std::size(tested_methods);
       ++method_index) {
    bool pass = true;
    CheckResizeMethodShouldAverageGrid(src,
                                       tested_methods[method_index],
                                       dest_w, dest_h, average_color,
                                       &pass);
    if (!pass) {
      all_methods_ok = false;
    }
  }

  if (!all_methods_ok) {
#if DEBUG_BITMAP_GENERATION
    SaveBitmapToPNG(src, "/tmp/ResizeShouldAverageColors_src.png");
#endif  // #if DEBUG_BITMAP_GENERATION
  }
}

static double sinc(double x) {
  if (x == 0.0) return 1.0;
  x *= std::numbers::pi;
  return sin(x) / x;
}

static double lanczos3(double offset) {
  if (fabs(offset) >= 3) return 0.0;
  return sinc(offset) * sinc(offset / 3.0);
}

TEST(ImageOperations, ScaleUp) {
  const int src_w = 3;
  const int src_h = 3;
  const int dst_w = 9;
  const int dst_h = 9;
  SkBitmap src;
  src.allocPixels(
      SkImageInfo::MakeN32Premul(src_w, src_h, SkColorSpace::MakeSRGB()));

  for (int src_y = 0; src_y < src_h; ++src_y) {
    for (int src_x = 0; src_x < src_w; ++src_x) {
      *src.getAddr32(src_x, src_y) =
          SkColorSetARGB(255, 10 + src_x * 100, 10 + src_y * 100, 0);
    }
  }

  SkBitmap dst = skia::ImageOperations::Resize(
      src,
      skia::ImageOperations::RESIZE_LANCZOS3,
      dst_w, dst_h);
  EXPECT_TRUE(dst.colorSpace() && dst.colorSpace()->isSRGB());
  for (int dst_y = 0; dst_y < dst_h; ++dst_y) {
    for (int dst_x = 0; dst_x < dst_w; ++dst_x) {
      float dst_x_in_src = (dst_x + 0.5) * src_w / dst_w;
      float dst_y_in_src = (dst_y + 0.5) * src_h / dst_h;
      float a = 0.0f;
      float r = 0.0f;
      float g = 0.0f;
      float b = 0.0f;
      float sum = 0.0f;
      for (int src_y = 0; src_y < src_h; ++src_y) {
        for (int src_x = 0; src_x < src_w; ++src_x) {
          double coeff =
              lanczos3(src_x + 0.5 - dst_x_in_src) *
              lanczos3(src_y + 0.5 - dst_y_in_src);
          sum += coeff;
          SkColor tmp = *src.getAddr32(src_x, src_y);
          a += coeff * SkColorGetA(tmp);
          r += coeff * SkColorGetR(tmp);
          g += coeff * SkColorGetG(tmp);
          b += coeff * SkColorGetB(tmp);
        }
      }
      a /= sum;
      r /= sum;
      g /= sum;
      b /= sum;
      if (a < 0.0f) a = 0.0f;
      if (r < 0.0f) r = 0.0f;
      if (g < 0.0f) g = 0.0f;
      if (b < 0.0f) b = 0.0f;
      if (a > 255.0f) a = 255.0f;
      if (r > 255.0f) r = 255.0f;
      if (g > 255.0f) g = 255.0f;
      if (b > 255.0f) b = 255.0f;
      SkColor dst_color = *dst.getAddr32(dst_x, dst_y);
      EXPECT_LE(fabs(SkColorGetA(dst_color) - a), 1.5f);
      EXPECT_LE(fabs(SkColorGetR(dst_color) - r), 1.5f);
      EXPECT_LE(fabs(SkColorGetG(dst_color) - g), 1.5f);
      EXPECT_LE(fabs(SkColorGetB(dst_color) - b), 1.5f);
      if (HasFailure()) {
        return;
      }
    }
  }
}
