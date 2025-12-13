// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_transform.h"

#include "base/functional/callback_helpers.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/client/test_shared_image_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/blink/renderer/platform/graphics/test/gpu_test_utils.h"

namespace blink {
namespace {

class StaticBitmapImageTransformTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_sii_ = base::MakeRefCounted<gpu::TestSharedImageInterface>();
    SharedGpuContext::Reset();
    test_context_provider_ = viz::TestContextProvider::CreateRaster();
    InitializeSharedGpuContextRaster(test_context_provider_.get());
  }

  scoped_refptr<AcceleratedStaticBitmapImage> CreateAccelerated(
      gfx::Size size,
      viz::SharedImageFormat format,
      SkAlphaType alpha_type,
      gfx::ColorSpace color_space) {
    auto client_si = test_sii_->CreateSharedImage(
        {format, size, color_space, kTopLeft_GrSurfaceOrigin, alpha_type,
         gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_DISPLAY_READ |
                                  gpu::SHARED_IMAGE_USAGE_RASTER_READ),
         "CanvasResourceRaster"},
        gpu::kNullSurfaceHandle);
    return AcceleratedStaticBitmapImage::CreateFromCanvasSharedImage(
        std::move(client_si), test_sii_->GenUnverifiedSyncToken(), alpha_type,
        SharedGpuContext::ContextProviderWrapper(),
        base::PlatformThread::CurrentRef(),
        base::MakeRefCounted<base::NullTaskRunner>(), base::DoNothing());
  }

  scoped_refptr<gpu::TestSharedImageInterface> test_sii_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<viz::TestContextProvider> test_context_provider_;
};

TEST_F(StaticBitmapImageTransformTest, ConvertColorSpace) {
  // Create an sRGB StaticBitmapImage.
  auto image =
      CreateAccelerated(gfx::Size(100, 100), GetN32FormatForCanvas(),
                        kPremul_SkAlphaType, gfx::ColorSpace::CreateSRGB());

  // A no-op color space conversion should not create a copy.
  auto image_srgb = StaticBitmapImageTransform::ConvertToColorSpace(
      image, gfx::ColorSpace::CreateSRGB().ToSkColorSpace());
  EXPECT_EQ(image_srgb, image);

  // A non-no-op color space conversion should create a copy, and the copy
  // should have been done by the GPU.
  auto image_p3 = StaticBitmapImageTransform::ConvertToColorSpace(
      image, gfx::ColorSpace::CreateDisplayP3D65().ToSkColorSpace());
  EXPECT_NE(image_p3, image);
  EXPECT_TRUE(image_p3->IsTextureBacked());
}

}  // namespace
}  // namespace blink
