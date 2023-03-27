// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/config/gpu_feature_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"

namespace gpu {
namespace {

constexpr GrSurfaceOrigin kSurfaceOrigin = kTopLeft_GrSurfaceOrigin;
constexpr SkAlphaType kAlphaType = kPremul_SkAlphaType;
constexpr auto kColorSpace = gfx::ColorSpace::CreateSRGB();
constexpr uint32_t kUsage = SHARED_IMAGE_USAGE_DISPLAY_READ |
                            SHARED_IMAGE_USAGE_RASTER |
                            SHARED_IMAGE_USAGE_CPU_UPLOAD;

class WrappedSkImageBackingFactoryTest
    : public SharedImageTestBase,
      public testing::WithParamInterface<viz::SharedImageFormat> {
 public:
  WrappedSkImageBackingFactoryTest() = default;
  ~WrappedSkImageBackingFactoryTest() override = default;

  viz::SharedImageFormat GetFormat() { return GetParam(); }

  void SetUp() override {
    // We don't use WrappedSkImage with ALPHA8 if it's GL context.
    // Note, that `gr_context_type` is not wired right now and is always GL.
    if (GetFormat() == viz::SinglePlaneFormat::kALPHA_8 &&
        gpu_preferences_.gr_context_type == GrContextType::kGL) {
      GTEST_SKIP();
    }

    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGL));

    backing_factory_ =
        std::make_unique<WrappedSkImageBackingFactory>(context_state_);
  }
};

// Verify creation and Skia access works as expected.
TEST_P(WrappedSkImageBackingFactoryTest, Basic) {
  auto format = GetFormat();
  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(100, 100);

  bool supported = backing_factory_->CanCreateSharedImage(
      kUsage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, {});
  ASSERT_TRUE(supported);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, gpu::kNullSurfaceHandle, size, kColorSpace,
      kSurfaceOrigin, kAlphaType, kUsage, /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  // Validate SkiaImageRepresentation works.
  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  EXPECT_TRUE(skia_representation);

  // Validate scoped write access works.
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  auto scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);

  ASSERT_TRUE(scoped_write_access);
  auto* surface = scoped_write_access->surface(/*plane_index=*/0);
  ASSERT_TRUE(surface);
  EXPECT_EQ(size.width(), surface->width());
  EXPECT_EQ(size.height(), surface->height());
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  scoped_write_access.reset();

  // Must be cleared before read access.
  skia_representation->SetCleared();

  // Validate scoped read access works.
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  auto* promise_texture =
      scoped_read_access->promise_image_texture(/*plane_index=*/0);
  EXPECT_TRUE(promise_texture);
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  GrBackendTexture backend_texture = promise_texture->backendTexture();
  EXPECT_TRUE(backend_texture.isValid());
  EXPECT_EQ(size.width(), backend_texture.width());
  EXPECT_EQ(size.height(), backend_texture.height());

  scoped_read_access.reset();
  skia_representation.reset();
}

// Verify that pixel upload works as expected.
TEST_P(WrappedSkImageBackingFactoryTest, Upload) {
  auto format = GetFormat();
  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(100, 100);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, gpu::kNullSurfaceHandle, size, kColorSpace,
      kSurfaceOrigin, kAlphaType, kUsage, /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  int num_planes = format.NumberOfPlanes();
  std::vector<SkBitmap> bitmaps(num_planes);
  for (int plane = 0; plane < num_planes; ++plane) {
    SkColorType color_type = ToClosestSkColorType(true, format, plane);
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    bitmaps[plane] = MakeRedBitmap(color_type, plane_size);
  }

  // Upload pixels and set cleared.
  ASSERT_TRUE(backing->UploadFromMemory(GetSkPixmaps(bitmaps)));
  backing->SetCleared();

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  // Validate SkiaImageRepresentation works.
  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);

  for (int plane = 0; plane < num_planes; ++plane) {
    auto* promise_texture = scoped_read_access->promise_image_texture(plane);
    ASSERT_TRUE(promise_texture);

    // Readback via Skia API and verify it's the same pixels that were uploaded.
    SkColorType color_type = ToClosestSkColorType(true, format, plane);
    auto sk_image = SkImages::BorrowTextureFrom(
        context_state_->gr_context(), promise_texture->backendTexture(),
        kSurfaceOrigin, color_type, kAlphaType, nullptr);
    ASSERT_TRUE(sk_image);

    SkImageInfo dst_info = bitmaps[plane].info();
    SkBitmap dst_bitmap;
    dst_bitmap.allocPixels(dst_info);
    EXPECT_TRUE(sk_image->readPixels(dst_info, dst_bitmap.getPixels(),
                                     dst_info.minRowBytes(), 0, 0));

    EXPECT_TRUE(cc::MatchesBitmap(dst_bitmap, bitmaps[plane],
                                  cc::ExactPixelComparator()));
  }

  scoped_read_access.reset();
  skia_representation.reset();
}

std::string TestParamToString(
    const testing::TestParamInfo<viz::SharedImageFormat>& param_info) {
  return param_info.param.ToTestParamString();
}

// BGRA_1010102 fails to create backing. BGRX_8888 and BGR_565 "work" but Skia
// just thinks is RGBX_8888 and RGB_565 respectively so upload doesn't work.
// TODO(kylechar): Add RGBA_F16 where it works.
const auto kFormats =
    ::testing::Values(viz::SinglePlaneFormat::kALPHA_8,
                      viz::SinglePlaneFormat::kR_8,
                      viz::SinglePlaneFormat::kRG_88,
                      viz::SinglePlaneFormat::kRGBA_4444,
                      viz::SinglePlaneFormat::kRGB_565,
                      viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_8888,
                      viz::SinglePlaneFormat::kRGBX_8888,
                      viz::SinglePlaneFormat::kRGBA_1010102,
                      viz::MultiPlaneFormat::kYUV_420_BIPLANAR,
                      viz::MultiPlaneFormat::kYVU_420);

INSTANTIATE_TEST_SUITE_P(,
                         WrappedSkImageBackingFactoryTest,
                         kFormats,
                         TestParamToString);

}  // namespace
}  // namespace gpu
