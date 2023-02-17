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
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

constexpr GrSurfaceOrigin kSurfaceOrigin = kTopLeft_GrSurfaceOrigin;
constexpr SkAlphaType kAlphaType = kPremul_SkAlphaType;
constexpr auto kColorSpace = gfx::ColorSpace::CreateSRGB();
constexpr uint32_t kUsage = SHARED_IMAGE_USAGE_DISPLAY_READ |
                            SHARED_IMAGE_USAGE_RASTER |
                            SHARED_IMAGE_USAGE_CPU_UPLOAD;

// Allocate a bitmap with red pixels. RED_8 will be filled with 0xFF repeating
// and RG_88 will be filled with OxFF00 repeating.
SkBitmap MakeRedBitmap(SkColorType color_type, const gfx::Size& size) {
  SkBitmap bitmap;
  bitmap.allocPixels(SkImageInfo::Make(size.width(), size.height(), color_type,
                                       kOpaque_SkAlphaType));

  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

std::vector<SkPixmap> GetSkPixmaps(const std::vector<SkBitmap>& bitmaps) {
  std::vector<SkPixmap> pixmaps;
  for (auto& bitmap : bitmaps) {
    pixmaps.push_back(bitmap.pixmap());
  }
  return pixmaps;
}

class WrappedSkImageBackingFactoryTest
    : public testing::TestWithParam<viz::SharedImageFormat> {
 public:
  WrappedSkImageBackingFactoryTest() = default;
  ~WrappedSkImageBackingFactoryTest() override {
    // |context_state_| must be destroyed while current.
    context_state_->MakeCurrent(surface_.get(), /*needs_gl=*/true);
  }

  viz::SharedImageFormat GetFormat() { return GetParam(); }

  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(),
                                                  gfx::Size());
    ASSERT_TRUE(surface_);

    auto context = gl::init::CreateGLContext(nullptr, surface_.get(),
                                             gl::GLContextAttribs());
    ASSERT_TRUE(context);
    bool result = context->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    context_state_ = base::MakeRefCounted<SharedContextState>(
        base::MakeRefCounted<gl::GLShareGroup>(), surface_, std::move(context),
        /*use_virtualized_gl_contexts=*/false, base::DoNothing());

    GpuDriverBugWorkarounds workarounds;
    scoped_refptr<gles2::FeatureInfo> feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    ASSERT_TRUE(context_state_->InitializeGrContext(GpuPreferences(),
                                                    workarounds, nullptr));
    ASSERT_TRUE(context_state_->InitializeGL(GpuPreferences(), feature_info));

    backing_factory_ =
        std::make_unique<WrappedSkImageBackingFactory>(context_state_);

    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
  }

 protected:
  MemoryTypeTracker memory_type_tracker_{nullptr};
  SharedImageManager shared_image_manager_{/*thread_safe=*/false};
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<WrappedSkImageBackingFactory> backing_factory_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
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
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
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
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
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
    auto sk_image = SkImage::MakeFromTexture(
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
