// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_2d_bitmap_provider.h"

#include "components/viz/common/resources/shared_image_format_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_color_params.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace blink {
namespace {

constexpr int kMaxTextureSize = 1024;

template <typename T>
SkImageInfo GetSkImageInfo(T* provider) {
  return SkImageInfo::Make(
      provider->Size().width(), provider->Size().height(),
      viz::ToClosestSkColorType(provider->GetSharedImageFormat()),
      provider->GetAlphaType(), provider->GetColorSpace().ToSkColorSpace());
}

class Canvas2DBitmapProviderTest : public testing::Test {
 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(Canvas2DBitmapProviderTest, Create) {
  const gfx::Size kSize(10, 10);
  const SkImageInfo kInfo =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = Canvas2DBitmapProvider::CreateForTesting(kSize, color_params);

  EXPECT_EQ(provider->Size(), kSize);
  EXPECT_TRUE(provider && provider->IsValid());
  EXPECT_TRUE(GetSkImageInfo(provider.get()) == kInfo);
}

TEST_F(Canvas2DBitmapProviderTest, HdrMetadata) {
  const gfx::Size kSize(10, 10);
  gfx::HDRMetadata hdr_metadata;
  hdr_metadata.extended_range = gfx::HdrMetadataExtendedRange(4.0f, 4.0f);

  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB, hdr_metadata,
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = Canvas2DBitmapProvider::CreateForTesting(kSize, color_params);
  EXPECT_TRUE(provider && provider->IsValid());
  scoped_refptr<StaticBitmapImage> snapshot = provider->Snapshot();
  EXPECT_TRUE(snapshot);
  EXPECT_EQ(snapshot->GetHdrMetadata(), hdr_metadata);
}

TEST_F(Canvas2DBitmapProviderTest, DimensionsExceedMaxTextureSize) {
  Canvas2DColorParams color_params(PredefinedColorSpace::kSRGB,
                                   gfx::HDRMetadata(),
                                   CanvasPixelFormat::kUint8,
                                   /*has_alpha=*/true);
  auto provider = Canvas2DBitmapProvider::CreateForTesting(
      gfx::Size(kMaxTextureSize - 1, kMaxTextureSize), color_params);
  EXPECT_TRUE(provider);
  provider = Canvas2DBitmapProvider::CreateForTesting(
      gfx::Size(kMaxTextureSize, kMaxTextureSize), color_params);
  EXPECT_TRUE(provider);
  provider = Canvas2DBitmapProvider::CreateForTesting(
      gfx::Size(kMaxTextureSize + 1, kMaxTextureSize), color_params);
  EXPECT_TRUE(provider);
}

}  // namespace
}  // namespace blink
