// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/rgba_to_yuva.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPixmap.h"

namespace skia {
namespace {

TEST(RGBAToYUVATest, Convert) {
  constexpr float kEpsilon = 0.01;
  struct TestPoint {
    SkColor4f color;
    SkAlphaType alpha_type;
    sk_sp<SkColorSpace> color_space;
    SkYUVColorSpace yuv_color_space;
  };

  // Several pairs of test points that will equal each other when called with
  // ConvertRGBAToOrFromYUVA.
  constexpr size_t kNumTests = 2;
  std::array<std::array<TestPoint, 2>, kNumTests> test_cases;
  test_cases[0] = {
      {
          {
              .color = {0.91748756f, 0.20028681f, 0.13856059f, 1.f},
              .alpha_type = kPremul_SkAlphaType,
              .color_space = SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB,
                                                   SkNamedGamut::kDisplayP3),
              .yuv_color_space = kIdentity_SkYUVColorSpace,
          },
          {
              .color = {0.245331f, 0.401317f, 0.941177f, 1.f},
              .alpha_type = kPremul_SkAlphaType,
              .color_space = SkColorSpace::MakeSRGB(),
              .yuv_color_space = kRec709_SkYUVColorSpace,
          },
      },
  };
  test_cases[1] = {
      {
          {
              .color = {0.5f, 0.f, 0.f, 0.5f},
              .alpha_type = kPremul_SkAlphaType,
              .color_space = SkColorSpace::MakeSRGB(),
              .yuv_color_space = kIdentity_SkYUVColorSpace,
          },
          {
              .color = {0.245331f, 0.401317f, 0.941177f, 0.5f},
              .alpha_type = kUnpremul_SkAlphaType,
              .color_space = SkColorSpace::MakeSRGB(),
              .yuv_color_space = kRec709_SkYUVColorSpace,
          },
      },
  };

  for (const auto& test_points : test_cases) {
    std::array<SkColor4f, 2> colors;
    std::array<SkPixmap, 2> pixmaps;
    // Set up the two `pixmaps` for the test case to be backed by `colors`
    for (size_t i = 0; i < 2; ++i) {
      pixmaps[i] = SkPixmap(SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType,
                                              test_points[i].alpha_type,
                                              test_points[i].color_space),
                            &colors[i], sizeof(SkColor4f));
    }

    for (size_t test_config = 0; test_config < 3; ++test_config) {
      size_t src_index = 0;
      size_t dst_index = 0;
      switch (test_config) {
        case 0:
          // Copy from pixmaps[0] to pixmaps[1].
          src_index = 0;
          dst_index = 1;
          break;
        case 1:
          // Copy from pixmaps[1] to pixmaps[0].
          src_index = 1;
          dst_index = 0;
          break;
        case 2:
          // Copy from pixmaps[0] to pixmaps[0] to ensure that copying to
          // oneself works.
          src_index = 0;
          dst_index = 0;
          break;
      }

      colors[dst_index] = SkColors::kTransparent;
      colors[src_index] = test_points[src_index].color;
      ConvertRGBAToOrFromYUVA(
          pixmaps[src_index], test_points[src_index].yuv_color_space,
          pixmaps[dst_index], test_points[dst_index].yuv_color_space);
      EXPECT_NEAR(colors[dst_index].fR, test_points[dst_index].color.fR,
                  kEpsilon);
      EXPECT_NEAR(colors[dst_index].fG, test_points[dst_index].color.fG,
                  kEpsilon);
      EXPECT_NEAR(colors[dst_index].fB, test_points[dst_index].color.fB,
                  kEpsilon);
      EXPECT_NEAR(colors[dst_index].fA, test_points[dst_index].color.fA,
                  kEpsilon);
    }
  }
}

}  // namespace
}  // namespace skia
