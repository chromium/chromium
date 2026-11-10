// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "skia/ext/rgba_to_yuva.h"

#include <array>
#include <cstdint>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkSurface.h"

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

// Opaque sRGB red through BT.2020 10-bit limited. Encoders read the 10 MSBs
// of each unorm16 sample (`value >> 6`).
TEST(RGBAToYUVATest, ConvertTo10Bit) {
  SkBitmap src_bitmap;
  src_bitmap.allocPixels(SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType,
                                           kOpaque_SkAlphaType,
                                           SkColorSpace::MakeSRGB()));
  src_bitmap.erase(SkColors::kRed, src_bitmap.bounds());
  const auto src_image = SkImages::RasterFromBitmap(src_bitmap);
  const auto color_space = SkColorSpace::MakeSRGB();
  const SkYUVAInfo yuva_info(
      SkISize::Make(1, 1), SkYUVAInfo::PlaneConfig::kY_UV,
      SkYUVAInfo::Subsampling::k444, kBT2020_10bit_Limited_SkYUVColorSpace);

  uint16_t y = 0;
  std::array<uint16_t, 2> uv = {};
  std::array<SkPixmap, 2> pixmaps = {
      SkPixmap(SkImageInfo::Make(1, 1, kR16_unorm_SkColorType,
                                 kOpaque_SkAlphaType, color_space),
               &y, sizeof(uint16_t)),
      SkPixmap(SkImageInfo::Make(1, 1, kR16G16_unorm_SkColorType,
                                 kOpaque_SkAlphaType, color_space),
               uv.data(), 2 * sizeof(uint16_t)),
  };
  std::array<sk_sp<SkSurface>, 2> surfaces;
  std::array<SkSurface*, 2> surface_ptrs;
  for (size_t i = 0; i < surfaces.size(); ++i) {
    surfaces[i] = SkSurfaces::WrapPixels(pixmaps[i]);
    surface_ptrs[i] = surfaces[i].get();
  }

  BlitRGBAToYUVA(src_image.get(), surface_ptrs, yuva_info);

  EXPECT_EQ(y >> 6, 294u);
  EXPECT_EQ(uv[0] >> 6, 387u);
  EXPECT_EQ(uv[1] >> 6, 960u);
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
    std::array<SkPixmap, SkYUVAInfo::kMaxPlanes> pms;
    for (size_t i = 0; i < test.bitmaps.size(); ++i) {
      pms[i] = test.bitmaps[i].pixmap();
    }
    const auto src =
        SkYUVAPixmaps::FromExternalPixmaps(test.yuva_info, pms.data());
    ASSERT_TRUE(src.isValid());

    // Read back into P3 to force a color space conversion.
    const auto dst_cs = SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB,
                                              SkNamedGamut::kDisplayP3);
    SkBitmap dst;
    dst.allocPixels(SkImageInfo::Make(kWidth, kHeight, kRGBA_F16_SkColorType,
                                      kPremul_SkAlphaType, dst_cs));

    ConvertYUVAToRGBA(src, test.bit_depth, dst.pixmap());

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

TEST(RGBAToYUVATest, OddDimensionsSubsampled) {
  const auto color_space = SkColorSpace::MakeSRGB();
  for (const SkISize size :
       {SkISize::Make(1, 1), SkISize::Make(3, 3), SkISize::Make(5, 3)}) {
    for (const auto subsampling :
         {SkYUVAInfo::Subsampling::k420, SkYUVAInfo::Subsampling::k422,
          SkYUVAInfo::Subsampling::k440, SkYUVAInfo::Subsampling::k410,
          SkYUVAInfo::Subsampling::k411}) {
      for (const bool use_src_image : {true, false}) {
        sk_sp<SkImage> src_image;
        if (use_src_image) {
          SkBitmap src_bitmap;
          src_bitmap.allocPixels(SkImageInfo::Make(
              size, kRGBA_8888_SkColorType, kOpaque_SkAlphaType, color_space));
          src_bitmap.erase(SkColors::kRed, src_bitmap.bounds());
          src_image = SkImages::RasterFromBitmap(src_bitmap);
        }

        const SkYUVAInfo yuva_info(size, SkYUVAInfo::PlaneConfig::kY_UV,
                                   subsampling,
                                   kRec709_Limited_SkYUVColorSpace);
        std::array<SkISize, SkYUVAInfo::kMaxPlanes> plane_dims;
        ASSERT_EQ(yuva_info.planeDimensions(plane_dims.data()), 2);

        SkBitmap y_bitmap;
        y_bitmap.allocPixels(
            SkImageInfo::Make(plane_dims[0], kR8_unorm_SkColorType,
                              kOpaque_SkAlphaType, color_space));
        y_bitmap.erase(SkColorSetARGB(0xFF, 0xAB, 0, 0), y_bitmap.bounds());

        SkBitmap uv_bitmap;
        uv_bitmap.allocPixels(
            SkImageInfo::Make(plane_dims[1], kR8G8_unorm_SkColorType,
                              kOpaque_SkAlphaType, color_space));
        uv_bitmap.erase(SkColorSetARGB(0xFF, 0xAB, 0xCD, 0),
                        uv_bitmap.bounds());

        std::array<sk_sp<SkSurface>, 2> surfaces = {
            SkSurfaces::WrapPixels(y_bitmap.pixmap()),
            SkSurfaces::WrapPixels(uv_bitmap.pixmap()),
        };
        std::array<SkSurface*, 2> surface_ptrs = {
            surfaces[0].get(),
            surfaces[1].get(),
        };

        BlitRGBAToYUVA(src_image.get(), surface_ptrs, yuva_info);

        const SkPixmap y_pixmap = y_bitmap.pixmap();
        const uint8_t expected_y =
            *static_cast<const uint8_t*>(y_pixmap.addr(0, 0));
        EXPECT_NE(expected_y, 0xABu);
        for (int y = 0; y < plane_dims[0].height(); ++y) {
          for (int x = 0; x < plane_dims[0].width(); ++x) {
            EXPECT_EQ(*static_cast<const uint8_t*>(y_pixmap.addr(x, y)),
                      expected_y);
          }
        }

        const SkPixmap uv_pixmap = uv_bitmap.pixmap();
        const uint16_t expected_uv =
            *static_cast<const uint16_t*>(uv_pixmap.addr(0, 0));
        EXPECT_NE(expected_uv, 0xCDABu);
        for (int y = 0; y < plane_dims[1].height(); ++y) {
          for (int x = 0; x < plane_dims[1].width(); ++x) {
            EXPECT_EQ(*static_cast<const uint16_t*>(uv_pixmap.addr(x, y)),
                      expected_uv)
                << "x=" << x << " y=" << y
                << " subsampling=" << static_cast<int>(subsampling)
                << " size=" << size.width() << "x" << size.height()
                << " use_src=" << use_src_image;
          }
        }
      }
    }
  }

  // Also verify strip-wise sub-rect blits and null-source clears on an
  // odd-sized alpha-planar subsampled target.
  constexpr int kWidth = 3;
  constexpr int kHeight = 3;
  const SkISize odd_size = SkISize::Make(kWidth, kHeight);
  const SkYUVAInfo yuva_alpha_info(odd_size, SkYUVAInfo::PlaneConfig::kY_UV_A,
                                   SkYUVAInfo::Subsampling::k420,
                                   kRec709_SkYUVColorSpace);
  std::array<SkISize, SkYUVAInfo::kMaxPlanes> alpha_plane_dims;
  ASSERT_EQ(yuva_alpha_info.planeDimensions(alpha_plane_dims.data()), 3);
  ASSERT_EQ(alpha_plane_dims[1], SkISize::Make(2, 2));

  SkBitmap y_bm;
  y_bm.allocPixels(SkImageInfo::Make(alpha_plane_dims[0], kR8_unorm_SkColorType,
                                     kUnpremul_SkAlphaType, color_space));
  SkBitmap uv_bm;
  uv_bm.allocPixels(SkImageInfo::Make(alpha_plane_dims[1],
                                      kR8G8_unorm_SkColorType,
                                      kUnpremul_SkAlphaType, color_space));
  SkBitmap a_bm;
  a_bm.allocPixels(SkImageInfo::Make(alpha_plane_dims[2], kR8_unorm_SkColorType,
                                     kUnpremul_SkAlphaType, color_space));

  std::array<sk_sp<SkSurface>, 3> alpha_surfaces = {
      SkSurfaces::WrapPixels(y_bm.pixmap()),
      SkSurfaces::WrapPixels(uv_bm.pixmap()),
      SkSurfaces::WrapPixels(a_bm.pixmap()),
  };
  std::array<SkSurface*, 3> alpha_surface_ptrs = {
      alpha_surfaces[0].get(),
      alpha_surfaces[1].get(),
      alpha_surfaces[2].get(),
  };

  y_bm.eraseColor(SK_ColorTRANSPARENT);
  uv_bm.eraseColor(SK_ColorTRANSPARENT);
  a_bm.eraseColor(SK_ColorTRANSPARENT);
  BlitRGBAToYUVA(/*src_image=*/nullptr, alpha_surface_ptrs, yuva_alpha_info,
                 SkRect::MakeXYWH(0, 1, kWidth, 1));
  const SkPixmap uv_alpha_pixmap = uv_bm.pixmap();
  const uint16_t cleared_uv =
      *static_cast<const uint16_t*>(uv_alpha_pixmap.addr(0, 0));
  EXPECT_NE(cleared_uv, 0u);
  EXPECT_EQ(*static_cast<const uint16_t*>(uv_alpha_pixmap.addr(1, 0)),
            cleared_uv);

  BlitRGBAToYUVA(/*src_image=*/nullptr, alpha_surface_ptrs, yuva_alpha_info,
                 SkRect::MakeXYWH(0, 0, kWidth, 1));
  BlitRGBAToYUVA(/*src_image=*/nullptr, alpha_surface_ptrs, yuva_alpha_info,
                 SkRect::MakeXYWH(0, 2, kWidth, 1));
  for (int y = 0; y < alpha_plane_dims[1].height(); ++y) {
    for (int x = 0; x < alpha_plane_dims[1].width(); ++x) {
      EXPECT_EQ(*static_cast<const uint16_t*>(uv_alpha_pixmap.addr(x, y)),
                cleared_uv)
          << "x=" << x << " y=" << y;
    }
  }
}

TEST(RGBAToYUVATest, SrcRegionSubsetClamping) {
  const auto color_space = SkColorSpace::MakeSRGB();
  // Create an 8x8 source bitmap: green background with a 4x4 red center at (2,
  // 2).
  SkBitmap src_bitmap;
  src_bitmap.allocPixels(SkImageInfo::Make(8, 8, kRGBA_8888_SkColorType,
                                           kOpaque_SkAlphaType, color_space));
  src_bitmap.erase(SkColors::kGreen, src_bitmap.bounds());
  const SkIRect src_sub_irect = SkIRect::MakeXYWH(2, 2, 4, 4);
  src_bitmap.erase(SkColors::kRed, src_sub_irect);
  const auto src_image = SkImages::RasterFromBitmap(src_bitmap);
  const SkRect src_region = SkRect::Make(src_sub_irect);

  // Destination is an odd-sized 3x3 NV12 image (plane 0: 3x3, plane 1: 2x2).
  const SkISize dst_size = SkISize::Make(3, 3);
  const SkYUVAInfo yuva_info(dst_size, SkYUVAInfo::PlaneConfig::kY_UV,
                             SkYUVAInfo::Subsampling::k420,
                             kRec709_Limited_SkYUVColorSpace);
  std::array<SkISize, SkYUVAInfo::kMaxPlanes> plane_dims;
  ASSERT_EQ(yuva_info.planeDimensions(plane_dims.data()), 2);

  // Also create a pure red reference image to obtain expected red Y and UV.
  SkBitmap ref_bitmap;
  ref_bitmap.allocPixels(SkImageInfo::Make(4, 4, kRGBA_8888_SkColorType,
                                           kOpaque_SkAlphaType, color_space));
  ref_bitmap.erase(SkColors::kRed, ref_bitmap.bounds());
  const auto ref_image = SkImages::RasterFromBitmap(ref_bitmap);

  SkBitmap ref_y_bm;
  SkBitmap ref_uv_bm;
  ref_y_bm.allocPixels(SkImageInfo::Make(plane_dims[0], kR8_unorm_SkColorType,
                                         kOpaque_SkAlphaType, color_space));
  ref_uv_bm.allocPixels(SkImageInfo::Make(plane_dims[1],
                                          kR8G8_unorm_SkColorType,
                                          kOpaque_SkAlphaType, color_space));
  std::array<sk_sp<SkSurface>, 2> ref_surfaces = {
      SkSurfaces::WrapPixels(ref_y_bm.pixmap()),
      SkSurfaces::WrapPixels(ref_uv_bm.pixmap()),
  };
  std::array<SkSurface*, 2> ref_surface_ptrs = {ref_surfaces[0].get(),
                                                ref_surfaces[1].get()};
  BlitRGBAToYUVA(ref_image.get(), ref_surface_ptrs, yuva_info);
  const uint8_t expected_red_y =
      *static_cast<const uint8_t*>(ref_y_bm.pixmap().addr(0, 0));
  const uint16_t expected_red_uv =
      *static_cast<const uint16_t*>(ref_uv_bm.pixmap().addr(0, 0));

  // Test blitting with src_region subset into an odd-sized target.
  SkBitmap dst_y_bm;
  SkBitmap dst_uv_bm;
  dst_y_bm.allocPixels(SkImageInfo::Make(plane_dims[0], kR8_unorm_SkColorType,
                                         kOpaque_SkAlphaType, color_space));
  dst_uv_bm.allocPixels(SkImageInfo::Make(plane_dims[1],
                                          kR8G8_unorm_SkColorType,
                                          kOpaque_SkAlphaType, color_space));
  dst_y_bm.erase(SkColorSetARGB(0xFF, 0xAB, 0, 0), dst_y_bm.bounds());
  dst_uv_bm.erase(SkColorSetARGB(0xFF, 0xAB, 0xCD, 0), dst_uv_bm.bounds());

  std::array<sk_sp<SkSurface>, 2> dst_surfaces = {
      SkSurfaces::WrapPixels(dst_y_bm.pixmap()),
      SkSurfaces::WrapPixels(dst_uv_bm.pixmap()),
  };
  std::array<SkSurface*, 2> dst_surface_ptrs = {dst_surfaces[0].get(),
                                                dst_surfaces[1].get()};

  BlitRGBAToYUVA(src_image.get(), dst_surface_ptrs, yuva_info,
                 /*dst_region=*/SkRect::MakeEmpty(),
                 /*clear_destination=*/false, src_region);

  // Every single texel in the destination planes must match expected red,
  // with no green bleed from outside src_region and full odd-dimension
  // coverage.
  const SkPixmap dst_y_pm = dst_y_bm.pixmap();
  for (int y = 0; y < plane_dims[0].height(); ++y) {
    for (int x = 0; x < plane_dims[0].width(); ++x) {
      EXPECT_EQ(*static_cast<const uint8_t*>(dst_y_pm.addr(x, y)),
                expected_red_y)
          << "Y plane mismatch at (" << x << ", " << y << ")";
    }
  }

  const SkPixmap dst_uv_pm = dst_uv_bm.pixmap();
  for (int y = 0; y < plane_dims[1].height(); ++y) {
    for (int x = 0; x < plane_dims[1].width(); ++x) {
      EXPECT_EQ(*static_cast<const uint16_t*>(dst_uv_pm.addr(x, y)),
                expected_red_uv)
          << "UV plane mismatch at (" << x << ", " << y << ")";
    }
  }
}

TEST(RGBAToYUVATest, DstRegionSubsetAndLetterbox) {
  const auto color_space = SkColorSpace::MakeSRGB();
  const SkISize dst_size = SkISize::Make(8, 8);
  const SkYUVAInfo yuva_info(dst_size, SkYUVAInfo::PlaneConfig::kY_UV,
                             SkYUVAInfo::Subsampling::k420,
                             kRec709_Limited_SkYUVColorSpace);
  std::array<SkISize, SkYUVAInfo::kMaxPlanes> plane_dims;
  ASSERT_EQ(yuva_info.planeDimensions(plane_dims.data()), 2);

  SkBitmap src_bitmap;
  src_bitmap.allocPixels(SkImageInfo::Make(4, 4, kRGBA_8888_SkColorType,
                                           kOpaque_SkAlphaType, color_space));
  src_bitmap.erase(SkColors::kRed, src_bitmap.bounds());
  const auto src_image = SkImages::RasterFromBitmap(src_bitmap);

  const SkRect dst_region = SkRect::MakeXYWH(2, 2, 4, 4);

  // Case 1: clear_destination = true
  {
    SkBitmap y_bm;
    SkBitmap uv_bm;
    y_bm.allocPixels(SkImageInfo::Make(plane_dims[0], kR8_unorm_SkColorType,
                                       kOpaque_SkAlphaType, color_space));
    uv_bm.allocPixels(SkImageInfo::Make(plane_dims[1], kR8G8_unorm_SkColorType,
                                        kOpaque_SkAlphaType, color_space));
    y_bm.erase(SkColorSetARGB(0xFF, 0xAB, 0, 0), y_bm.bounds());
    uv_bm.erase(SkColorSetARGB(0xFF, 0xAB, 0xCD, 0), uv_bm.bounds());

    std::array<sk_sp<SkSurface>, 2> surfaces = {
        SkSurfaces::WrapPixels(y_bm.pixmap()),
        SkSurfaces::WrapPixels(uv_bm.pixmap()),
    };
    std::array<SkSurface*, 2> surface_ptrs = {surfaces[0].get(),
                                              surfaces[1].get()};

    BlitRGBAToYUVA(src_image.get(), surface_ptrs, yuva_info, dst_region,
                   /*clear_destination=*/true);

    const SkPixmap y_pm = y_bm.pixmap();
    const SkPixmap uv_pm = uv_bm.pixmap();

    // Outside dst_region (e.g. at (0, 0)), pixels should be cleared to black,
    // not marker 0xAB / 0xCDAB.
    EXPECT_NE(*static_cast<const uint8_t*>(y_pm.addr(0, 0)), 0xABu);
    EXPECT_NE(*static_cast<const uint16_t*>(uv_pm.addr(0, 0)), 0xCDABu);

    // Inside dst_region: at (2, 2) in Y plane, (1, 1) in UV plane.
    const uint8_t red_y = *static_cast<const uint8_t*>(y_pm.addr(2, 2));
    const uint16_t red_uv = *static_cast<const uint16_t*>(uv_pm.addr(1, 1));
    EXPECT_NE(red_y, *static_cast<const uint8_t*>(y_pm.addr(0, 0)));
    EXPECT_NE(red_uv, *static_cast<const uint16_t*>(uv_pm.addr(0, 0)));
  }

  // Case 2: clear_destination = false
  {
    SkBitmap y_bm;
    SkBitmap uv_bm;
    y_bm.allocPixels(SkImageInfo::Make(plane_dims[0], kR8_unorm_SkColorType,
                                       kOpaque_SkAlphaType, color_space));
    uv_bm.allocPixels(SkImageInfo::Make(plane_dims[1], kR8G8_unorm_SkColorType,
                                        kOpaque_SkAlphaType, color_space));
    y_bm.erase(SkColorSetARGB(0xFF, 0xAB, 0, 0), y_bm.bounds());
    uv_bm.erase(SkColorSetARGB(0xFF, 0xAB, 0xCD, 0), uv_bm.bounds());

    std::array<sk_sp<SkSurface>, 2> surfaces = {
        SkSurfaces::WrapPixels(y_bm.pixmap()),
        SkSurfaces::WrapPixels(uv_bm.pixmap()),
    };
    std::array<SkSurface*, 2> surface_ptrs = {surfaces[0].get(),
                                              surfaces[1].get()};

    BlitRGBAToYUVA(src_image.get(), surface_ptrs, yuva_info, dst_region,
                   /*clear_destination=*/false);

    const SkPixmap y_pm = y_bm.pixmap();
    const SkPixmap uv_pm = uv_bm.pixmap();

    // Outside dst_region (e.g. at (0, 0)), pixels should remain untouched
    // marker bytes.
    EXPECT_EQ(*static_cast<const uint8_t*>(y_pm.addr(0, 0)), 0xABu);
    EXPECT_EQ(*static_cast<const uint16_t*>(uv_pm.addr(0, 0)), 0xCDABu);

    // Inside dst_region: at (2, 2) in Y plane, (1, 1) in UV plane, pixels are
    // blitted.
    EXPECT_NE(*static_cast<const uint8_t*>(y_pm.addr(2, 2)), 0xABu);
    EXPECT_NE(*static_cast<const uint16_t*>(uv_pm.addr(1, 1)), 0xCDABu);
  }
}

}  // namespace
}  // namespace skia
