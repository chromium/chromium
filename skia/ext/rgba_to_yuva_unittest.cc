// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/rgba_to_yuva.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
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
  // ConvertRGBAToYUVA.
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

    for (size_t test_config = 0; test_config < 2; ++test_config) {
      size_t src_index = 0;
      size_t dst_index = 0;
      switch (test_config) {
        case 0:
          // Copy from pixmaps[0] to pixmaps[1].
          src_index = 0;
          dst_index = 1;
          break;
        case 1:
          // Copy from pixmaps[0] to pixmaps[0] to ensure that copying to
          // oneself works.
          src_index = 0;
          dst_index = 0;
          break;
      }

      colors[dst_index] = SkColors::kTransparent;
      colors[src_index] = test_points[src_index].color;
      ConvertRGBAToYUVA(pixmaps[src_index],
                        SkYUVAInfo(pixmaps[dst_index].dimensions(),
                                   SkYUVAInfo::PlaneConfig::kYUVA,
                                   SkYUVAInfo::Subsampling::k444,
                                   test_points[dst_index].yuv_color_space),
                        {pixmaps[dst_index]});
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

TEST(RGBAToYUVA, PlaneConfig) {
  constexpr float kEpsilon = 0.01;
  struct TestPoint {
    SkColor4f color;
    SkAlphaType alpha_type;
    sk_sp<SkColorSpace> color_space;
    SkYUVColorSpace yuv_color_space;
  };

  constexpr size_t kNumTestPoints = 2;
  std::array<TestPoint, kNumTestPoints> src_points({
      {
          .color = {0.91748756f, 0.20028681f, 0.13856059f, 1.f},
          .alpha_type = kPremul_SkAlphaType,
          .color_space = SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB,
                                               SkNamedGamut::kDisplayP3),
          .yuv_color_space = kIdentity_SkYUVColorSpace,
      },
      {
          .color = {0.5f, 0.f, 0.f, 0.5f},
          .alpha_type = kPremul_SkAlphaType,
          .color_space = SkColorSpace::MakeSRGB(),
          .yuv_color_space = kIdentity_SkYUVColorSpace,
      },
  });
  std::array<TestPoint, kNumTestPoints> dst_points({
      {
          .color = {0.245331f, 0.401317f, 0.941177f, 1.f},
          .alpha_type = kPremul_SkAlphaType,
          .color_space = SkColorSpace::MakeSRGB(),
          .yuv_color_space = kRec709_SkYUVColorSpace,
      },
      {
          .color = {0.245331f, 0.401317f, 0.941177f, 0.5f},
          // This is a very important test configuration. The individual planes
          // are opaque, but this verifies that they are internally changed to
          // unpremultiplied when blitting.
          .alpha_type = kOpaque_SkAlphaType,
          .color_space = SkColorSpace::MakeSRGB(),
          .yuv_color_space = kRec709_SkYUVColorSpace,
      },
  });

  struct Config {
    SkYUVAInfo::PlaneConfig plane_config;
    const char* name;
  };
  std::array<std::vector<Config>, kNumTestPoints> configs({
      {
          {SkYUVAInfo::PlaneConfig::kY_U_V, "Y_U_V"},
          {SkYUVAInfo::PlaneConfig::kY_UV, "Y_UV"},
          {SkYUVAInfo::PlaneConfig::kY_U_V_A, "Y_U_V_A"},
          {SkYUVAInfo::PlaneConfig::kY_UV_A, "Y_UV_A"},
      },
      {
          {SkYUVAInfo::PlaneConfig::kY_U_V_A, "Y_U_V_A"},
          {SkYUVAInfo::PlaneConfig::kY_UV_A, "Y_UV_A"},
      },
  });

  for (size_t t = 0; t < kNumTestPoints; ++t) {
    const auto& src_point = src_points[t];
    const auto& dst_point = dst_points[t];

    SkPixmap src_pixmap(
        SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, src_point.alpha_type,
                          src_point.color_space),
        &src_point.color, sizeof(SkColor4f));

    for (const auto& config : configs[t]) {
      SkYUVAInfo yuva_info(src_pixmap.dimensions(), config.plane_config,
                           SkYUVAInfo::Subsampling::k444,
                           dst_point.yuv_color_space);
      int num_planes = yuva_info.numPlanes();
      std::vector<SkColor4f> plane_colors(num_planes, SkColors::kTransparent);
      std::vector<SkPixmap> plane_pixmaps(num_planes);
      for (int i = 0; i < num_planes; ++i) {
        plane_pixmaps[i] = SkPixmap(
            SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, dst_point.alpha_type,
                              dst_point.color_space),
            &plane_colors[i], sizeof(SkColor4f));
      }

      ConvertRGBAToYUVA(src_pixmap, yuva_info, plane_pixmaps);

      const auto expected = dst_point.color;
      EXPECT_NEAR(plane_colors[0].fR, expected.fR, kEpsilon) << config.name;
      switch (config.plane_config) {
        case SkYUVAInfo::PlaneConfig::kY_U_V:
          EXPECT_NEAR(plane_colors[1].fR, expected.fG, kEpsilon) << config.name;
          EXPECT_NEAR(plane_colors[2].fR, expected.fB, kEpsilon) << config.name;
          break;
        case SkYUVAInfo::PlaneConfig::kY_UV:
          EXPECT_NEAR(plane_colors[1].fR, expected.fG, kEpsilon) << config.name;
          EXPECT_NEAR(plane_colors[1].fG, expected.fB, kEpsilon) << config.name;
          break;
        case SkYUVAInfo::PlaneConfig::kY_U_V_A:
          EXPECT_NEAR(plane_colors[1].fR, expected.fG, kEpsilon) << config.name;
          EXPECT_NEAR(plane_colors[2].fR, expected.fB, kEpsilon) << config.name;
          EXPECT_NEAR(plane_colors[3].fR, expected.fA, kEpsilon) << config.name;
          break;
        case SkYUVAInfo::PlaneConfig::kY_UV_A:
          EXPECT_NEAR(plane_colors[1].fR, expected.fG, kEpsilon) << config.name;
          EXPECT_NEAR(plane_colors[1].fG, expected.fB, kEpsilon) << config.name;
          EXPECT_NEAR(plane_colors[2].fR, expected.fA, kEpsilon) << config.name;
          break;
        default:
          FAIL() << "Unsupported plane config";
      }
    }
  }
}

TEST(RGBAToYUVATest, Basic) {
  // The test color is sRGB red.

  // This value, when used with sRGB primariees and transfer, and
  // kRec709_SkYUVColorSpace YUV to RGB matrix, will match sRGB red.
  const SkColor4f kTestColorRec709YUV = {0.245331f, 0.401317f, 0.941177f, 1.f};

  // This value, when used with BT2020 primaries, 2.2 gamma transfer, and
  // kBT2020_10bit_Limited_SkYUVColorSpace YUV to RGB matrix, will
  // match sRGB red.
  const SkColor4f kTestColorRec2020YUV = {0.37856f, 0.36303f, 0.75170f, 1.f};

  constexpr int kWidth = 20;
  constexpr int kHeight = 20;
  const auto kSize = SkISize::Make(kWidth, kHeight);

  struct TestConfig {
    // The source YUVA info.
    SkYUVAInfo yuva_info;
    // SkBitmaps for the source planes.
    std::vector<SkBitmap> bitmaps;
    // The actual bit depth for the source planes.
    int bit_depth;
    // The expected resulting color when converted to sRGB.
    SkColor4f srgb_color;
  };
  std::vector<TestConfig> tests;

  // RGBA (not a valid YUVA space).
  {
    TestConfig config = {
        .srgb_color = SkColors::kRed,
    };
    config.bitmaps.resize(1);
    config.bitmaps[0].allocPixels(SkImageInfo::Make(
        kWidth, kHeight, kN32_SkColorType, kPremul_SkAlphaType));
    config.bitmaps[0].erase({1.f, 0.f, 0.f, 1.f}, config.bitmaps[0].bounds());
    for (auto& bm : config.bitmaps) {
      bm.setColorSpace(SkColorSpace::MakeSRGB());
    }
    tests.push_back(config);
  }

  // I420
  {
    TestConfig config = {
        .yuva_info =
            SkYUVAInfo(kSize, SkYUVAInfo::PlaneConfig::kY_U_V,
                       SkYUVAInfo::Subsampling::k420, kRec709_SkYUVColorSpace),
        .bit_depth = 8,
        .srgb_color = SkColors::kRed,
    };
    config.bitmaps.resize(config.yuva_info.numPlanes());
    config.bitmaps[0].allocPixels(SkImageInfo::Make(
        kWidth, kHeight, kR8_unorm_SkColorType, kOpaque_SkAlphaType));
    config.bitmaps[1].allocPixels(SkImageInfo::Make(
        kWidth / 2, kHeight / 2, kR8_unorm_SkColorType, kOpaque_SkAlphaType));
    config.bitmaps[2].allocPixels(SkImageInfo::Make(
        kWidth / 2, kHeight / 2, kR8_unorm_SkColorType, kOpaque_SkAlphaType));

    config.bitmaps[0].erase({kTestColorRec709YUV.fR, 0.f, 0.f, 1.f},
                            config.bitmaps[0].bounds());
    config.bitmaps[1].erase({kTestColorRec709YUV.fG, 0.f, 0.f, 1.f},
                            config.bitmaps[1].bounds());
    config.bitmaps[2].erase({kTestColorRec709YUV.fB, 0.f, 0.f, 1.f},
                            config.bitmaps[2].bounds());
    for (auto& bm : config.bitmaps) {
      bm.setColorSpace(SkColorSpace::MakeSRGB());
    }

    tests.push_back(config);
  }

  // NV12
  {
    TestConfig config = {
        .yuva_info =
            SkYUVAInfo(kSize, SkYUVAInfo::PlaneConfig::kY_UV,
                       SkYUVAInfo::Subsampling::k420, kRec709_SkYUVColorSpace),
        .bit_depth = 8,
        .srgb_color = SkColors::kRed,
    };

    config.bitmaps.resize(config.yuva_info.numPlanes());
    config.bitmaps[0].allocPixels(SkImageInfo::Make(
        kWidth, kHeight, kR8_unorm_SkColorType, kOpaque_SkAlphaType));
    config.bitmaps[1].allocPixels(SkImageInfo::Make(
        kWidth / 2, kHeight / 2, kR8G8_unorm_SkColorType, kOpaque_SkAlphaType));

    config.bitmaps[0].erase({kTestColorRec709YUV.fR, 0.f, 0.f, 1.f},
                            config.bitmaps[0].bounds());
    config.bitmaps[1].erase(
        {kTestColorRec709YUV.fG, kTestColorRec709YUV.fB, 0.f, 1.f},
        config.bitmaps[1].bounds());
    for (auto& bm : config.bitmaps) {
      bm.setColorSpace(SkColorSpace::MakeSRGB());
    }

    tests.push_back(config);
  }

  // 10-bit Y-U-V
  {
    TestConfig config = {
        .yuva_info = SkYUVAInfo(kSize, SkYUVAInfo::PlaneConfig::kY_U_V,
                                SkYUVAInfo::Subsampling::k420,
                                kBT2020_10bit_Limited_SkYUVColorSpace),
        .bit_depth = 10,
        .srgb_color = SkColors::kRed,
    };
    constexpr float s = 1023.f / 65535.f;
    config.bitmaps.resize(config.yuva_info.numPlanes());
    config.bitmaps[0].allocPixels(SkImageInfo::Make(
        kWidth, kHeight, kR16_unorm_SkColorType, kOpaque_SkAlphaType));
    config.bitmaps[1].allocPixels(SkImageInfo::Make(
        kWidth / 2, kHeight / 2, kR16_unorm_SkColorType, kOpaque_SkAlphaType));
    config.bitmaps[2].allocPixels(SkImageInfo::Make(
        kWidth / 2, kHeight / 2, kR16_unorm_SkColorType, kOpaque_SkAlphaType));

    config.bitmaps[0].erase({s * kTestColorRec2020YUV.fR, 0.f, 0.f, 1.f},
                            config.bitmaps[0].bounds());
    config.bitmaps[1].erase({s * kTestColorRec2020YUV.fG, 0.f, 0.f, 1.f},
                            config.bitmaps[1].bounds());
    config.bitmaps[2].erase({s * kTestColorRec2020YUV.fB, 0.f, 0.f, 1.f},
                            config.bitmaps[2].bounds());
    for (auto& bm : config.bitmaps) {
      bm.setColorSpace(SkColorSpace::MakeRGB(SkNamedTransferFn::kRec2020,
                                             SkNamedGamut::kRec2020));
    }
    tests.push_back(config);
  }

  // 10-bit Y-UV-A
  {
    constexpr float kAlpha = 512.f / 1023.f;
    TestConfig config = {
        .yuva_info = SkYUVAInfo(kSize, SkYUVAInfo::PlaneConfig::kY_UV_A,
                                SkYUVAInfo::Subsampling::k420,
                                kBT2020_10bit_Limited_SkYUVColorSpace),
        .bit_depth = 10,
        .srgb_color = {1.f, 0.f, 0.f, kAlpha},
    };

    const float s = 1023.f / 65535.f;
    config.bitmaps.resize(config.yuva_info.numPlanes());
    config.bitmaps[0].allocPixels(SkImageInfo::Make(
        kWidth, kHeight, kR16_unorm_SkColorType, kOpaque_SkAlphaType));
    config.bitmaps[1].allocPixels(SkImageInfo::Make(kWidth / 2, kHeight / 2,
                                                    kR16G16_unorm_SkColorType,
                                                    kOpaque_SkAlphaType));
    config.bitmaps[2].allocPixels(SkImageInfo::Make(
        kWidth, kHeight, kR16_unorm_SkColorType, kOpaque_SkAlphaType));

    config.bitmaps[0].erase({s * kTestColorRec2020YUV.fR, 0.f, 0.f, 1.f},
                            config.bitmaps[0].bounds());
    config.bitmaps[1].erase(
        {s * kTestColorRec2020YUV.fG, s * kTestColorRec2020YUV.fB, 0.f, 1.f},
        config.bitmaps[1].bounds());
    config.bitmaps[2].erase({s * kAlpha, 0.f, 0.f, 1.f},
                            config.bitmaps[2].bounds());
    for (auto& bm : config.bitmaps) {
      bm.setColorSpace(SkColorSpace::MakeRGB(SkNamedTransferFn::kRec2020,
                                             SkNamedGamut::kRec2020));
    }

    tests.push_back(config);
  }

  // 12-bit Y-UV
  {
    TestConfig config = {
        .yuva_info =
            SkYUVAInfo(kSize, SkYUVAInfo::PlaneConfig::kY_UV,
                       SkYUVAInfo::Subsampling::k420, kRec709_SkYUVColorSpace),
        .bit_depth = 12,
        .srgb_color = SkColors::kRed,
    };

    const float s = 4095.f / 65535.f;
    config.bitmaps.resize(config.yuva_info.numPlanes());
    config.bitmaps[0].allocPixels(SkImageInfo::Make(
        kWidth, kHeight, kR16_unorm_SkColorType, kOpaque_SkAlphaType));
    config.bitmaps[1].allocPixels(SkImageInfo::Make(kWidth / 2, kHeight / 2,
                                                    kR16G16_unorm_SkColorType,
                                                    kOpaque_SkAlphaType));

    config.bitmaps[0].erase({s * kTestColorRec709YUV.fR, 0.f, 0.f, 1.f},
                            config.bitmaps[0].bounds());
    config.bitmaps[1].erase(
        {s * kTestColorRec709YUV.fG, s * kTestColorRec709YUV.fB, 0.f, 1.f},
        config.bitmaps[1].bounds());
    for (auto& bm : config.bitmaps) {
      bm.setColorSpace(SkColorSpace::MakeSRGB());
    }

    tests.push_back(config);
  }

  // For all of the configs, call ConvertYUVAToRGBA, and ensure that the
  // resulting color matches the expected color.
  for (const auto& test : tests) {
    std::vector<SkPixmap> pms;
    for (const auto& bm : test.bitmaps) {
      pms.push_back(bm.pixmap());
    }

    // Read back into P3 to force a color space conversion.
    const auto dst_cs = SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB,
                                              SkNamedGamut::kDisplayP3);
    SkBitmap dst;
    dst.allocPixels(SkImageInfo::Make(kWidth, kHeight, kRGBA_F16_SkColorType,
                                      kPremul_SkAlphaType, dst_cs));

    ConvertYUVAToRGBA(test.yuva_info, test.bit_depth, pms, dst.pixmap());

    // Convert to sRGB for comparison.
    SkColor4f result;
    SkPixmap result_pm(
        SkImageInfo::Make(1, 1, kRGBA_F32_SkColorType, kUnpremul_SkAlphaType,
                          SkColorSpace::MakeSRGB()),
        &result, sizeof(result));
    for (int x = 0; x < 2; ++x) {
      for (int y = 0; y < 2; ++y) {
        int xx = x ? kWidth - 1 : x;
        int yy = y ? kHeight - 1 : y;
        EXPECT_TRUE(dst.readPixels(result_pm, xx, yy));

        const float kEpsilon = 1.5f / 255.f;
        EXPECT_NEAR(result.fR, test.srgb_color.fR, kEpsilon);
        EXPECT_NEAR(result.fG, test.srgb_color.fG, kEpsilon);
        EXPECT_NEAR(result.fB, test.srgb_color.fB, kEpsilon);
        EXPECT_NEAR(result.fA, test.srgb_color.fA, kEpsilon);
      }
    }
  }
}

}  // namespace
}  // namespace skia
