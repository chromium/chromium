// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"

#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
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
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"

namespace gpu {
namespace {

constexpr GrSurfaceOrigin kSurfaceOrigin = kTopLeft_GrSurfaceOrigin;
constexpr SkAlphaType kAlphaType = kPremul_SkAlphaType;
constexpr auto kColorSpace = gfx::ColorSpace::CreateSRGB();
constexpr SharedImageUsageSet kUsage =
    SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_RASTER_READ |
    SHARED_IMAGE_USAGE_RASTER_WRITE | SHARED_IMAGE_USAGE_CPU_UPLOAD;

class WrappedSkImageBackingFactoryTest
    : public SharedImageTestBase,
      public testing::WithParamInterface<
          std::tuple<viz::SharedImageFormat, GrContextType>> {
 public:
  WrappedSkImageBackingFactoryTest() = default;
  ~WrappedSkImageBackingFactoryTest() override = default;

  viz::SharedImageFormat GetFormat() { return std::get<0>(GetParam()); }
  GrContextType GetGrContextType() { return std::get<1>(GetParam()); }

  void SetUp() override {
    auto gr_context_type = GetGrContextType();
    if (gr_context_type == GrContextType::kGraphiteDawn &&
        !IsGraphiteDawnSupported()) {
      GTEST_SKIP() << "Graphite/Dawn not supported";
    }
    ASSERT_NO_FATAL_FAILURE(InitializeContext(gr_context_type));

    auto format = GetFormat();
    if (gr_context_type == GrContextType::kGL &&
        format == viz::SinglePlaneFormat::kBGRA_8888 &&
        !context_state_->feature_info()
             ->feature_flags()
             .ext_texture_format_bgra8888) {
      // We don't support GL context with Dawn for now.
      GTEST_SKIP();
    }

    // We don't use WrappedSkImageBacking with ALPHA8 if it's GL context.
    if (format == viz::SinglePlaneFormat::kALPHA_8 &&
        gr_context_type == GrContextType::kGL) {
      GTEST_SKIP();
    }

    // We don't support RGBA_4444 and RGB_565 formats with
    // WrappedGraphiteTextureBacking.
    if (gr_context_type == GrContextType::kGraphiteDawn) {
      // Formats not supported with Dawn for now.
      if (format == viz::SinglePlaneFormat::kRGBA_4444 ||
          format == viz::SinglePlaneFormat::kRGB_565) {
        GTEST_SKIP();
      }
    }

    backing_factory_ =
        std::make_unique<WrappedSkImageBackingFactory>(context_state_);
  }
};

// Verify creation and Skia access works as expected.
TEST_P(WrappedSkImageBackingFactoryTest, Basic) {
  auto format = GetFormat();
  auto mailbox = Mailbox::Generate();
  gfx::Size size(100, 100);

  bool supported = backing_factory_->CanCreateSharedImage(
      kUsage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      gr_context_type(), {});
  ASSERT_TRUE(supported);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, gpu::kNullSurfaceHandle, size, kColorSpace,
      kSurfaceOrigin, kAlphaType, kUsage, "TestLabel",
      /*is_thread_safe=*/false);
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
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());

  for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
    auto* surface = scoped_write_access->surface(plane);
    ASSERT_TRUE(surface);

    auto plane_size = format.GetPlaneSize(plane, size);
    EXPECT_EQ(plane_size.width(), surface->width());
    EXPECT_EQ(plane_size.height(), surface->height());
  }

  scoped_write_access.reset();

  // Must be cleared before read access.
  skia_representation->SetCleared();

  // Validate scoped read access works.
  std::unique_ptr<SkiaImageRepresentation::ScopedReadAccess> scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());

  if (gr_context()) {
    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      auto* promise_texture = scoped_read_access->promise_image_texture(plane);
      ASSERT_TRUE(promise_texture);
      GrBackendTexture backend_texture = promise_texture->backendTexture();
      EXPECT_TRUE(backend_texture.isValid());

      auto plane_size = format.GetPlaneSize(plane, size);
      EXPECT_EQ(plane_size.width(), backend_texture.width());
      EXPECT_EQ(plane_size.height(), backend_texture.height());
    }
  } else {
    ASSERT_TRUE(context_state_->graphite_context());
    for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
      auto graphite_texture = scoped_read_access->graphite_texture(plane);
      EXPECT_TRUE(graphite_texture.isValid());

      auto plane_size = format.GetPlaneSize(plane, size);
      EXPECT_EQ(plane_size.width(), graphite_texture.dimensions().width());
      EXPECT_EQ(plane_size.height(), graphite_texture.dimensions().height());
    }
  }

  scoped_read_access.reset();
  skia_representation.reset();
}

// Verify that pixel upload works as expected.
TEST_P(WrappedSkImageBackingFactoryTest, Upload) {
  auto format = GetFormat();
  auto mailbox = Mailbox::Generate();
  gfx::Size size(100, 100);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, gpu::kNullSurfaceHandle, size, kColorSpace,
      kSurfaceOrigin, kAlphaType, kUsage, "TestLabel",
      /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  std::vector<SkBitmap> bitmaps = AllocateRedBitmaps(format, size);

  // Upload pixels and set cleared.
  ASSERT_TRUE(backing->UploadFromMemory(GetSkPixmaps(bitmaps)));
  backing->SetCleared();

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  VerifyPixelsWithReadback(mailbox, bitmaps);
}

std::string TestParamToString(
    const testing::TestParamInfo<
        std::tuple<viz::SharedImageFormat, GrContextType>>& param_info) {
  std::string format = std::get<0>(param_info.param).ToTestParamString();
  std::string context_type =
      (std::get<1>(param_info.param) == GrContextType::kGL) ? "GL"
                                                            : "GraphiteDawn";
  return context_type + "_" + format;
}

// BGRA_1010102 fails to create backing. BGRX_8888 and BGR_565 "work" but Skia
// just thinks is RGBX_8888 and RGB_565 respectively so upload doesn't work.
// TODO(kylechar): Add RGBA_F16 where it works.
const auto kFormats = ::testing::Values(viz::SinglePlaneFormat::kALPHA_8,
                                        viz::SinglePlaneFormat::kR_8,
                                        viz::SinglePlaneFormat::kRG_88,
                                        viz::SinglePlaneFormat::kRGBA_4444,
                                        viz::SinglePlaneFormat::kRGB_565,
                                        viz::SinglePlaneFormat::kRGBA_8888,
                                        viz::SinglePlaneFormat::kBGRA_8888,
                                        viz::SinglePlaneFormat::kRGBX_8888,
                                        viz::SinglePlaneFormat::kRGBA_1010102,
                                        viz::MultiPlaneFormat::kNV12,
                                        viz::MultiPlaneFormat::kYV12,
                                        viz::MultiPlaneFormat::kI420);

INSTANTIATE_TEST_SUITE_P(
    ,
    WrappedSkImageBackingFactoryTest,
    testing::Combine(kFormats,
                     testing::Values(GrContextType::kGL,
                                     GrContextType::kGraphiteDawn)),
    TestParamToString);

}  // namespace
}  // namespace gpu
