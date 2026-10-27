// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/wrapped_sk_image_backing_factory.h"

#include <array>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_info.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorType.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/ganesh/SkImageGanesh.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gfx/skia_span_util.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

constexpr GrSurfaceOrigin kSurfaceOrigin = kTopLeft_GrSurfaceOrigin;
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

    // We don't support RGBA_4444 as format is not supported with Dawn.
    if (gr_context_type == GrContextType::kGraphiteDawn &&
        format == viz::SinglePlaneFormat::kRGBA_4444) {
      GTEST_SKIP();
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

  const SkAlphaType alpha_type = GetAlphaType(format);
  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, kColorSpace, kSurfaceOrigin, alpha_type, kUsage,
       "TestLabel"},
      gpu::kNullSurfaceHandle, /*is_thread_safe=*/false);
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
    ASSERT_TRUE(context_state_->graphite_shared_context());
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
      mailbox,
      {format, size, kColorSpace, kSurfaceOrigin, GetAlphaType(format), kUsage,
       "TestLabel"},
      gpu::kNullSurfaceHandle, /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  std::vector<SkBitmap> bitmaps = AllocateRedBitmaps(format, size);

  // Upload pixels and set cleared.
  ASSERT_TRUE(backing->UploadFromMemory(GetSkPixmaps(bitmaps)));
  backing->SetCleared();

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  VerifyPixelsWithReadback(mailbox, bitmaps);
}

TEST_P(WrappedSkImageBackingFactoryTest, UploadAndReadback) {
  auto format = GetFormat();
  auto mailbox = Mailbox::Generate();
  gfx::Size size(100, 100);

  const SkAlphaType alpha_type = GetAlphaType(format);
  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, kColorSpace, kSurfaceOrigin, alpha_type, kUsage,
       "TestLabel"},
      gpu::kNullSurfaceHandle, /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  std::vector<SkBitmap> upload_bitmaps = AllocateRedBitmaps(format, size);
  std::vector<SkPixmap> upload_pixmaps = GetSkPixmaps(upload_bitmaps);

  // Upload.
  ASSERT_TRUE(backing->UploadFromMemory(upload_pixmaps));
  backing->SetCleared();

  // Allocate destination bitmaps.
  int num_planes = format.NumberOfPlanes();
  std::vector<SkBitmap> readback_bitmaps(num_planes);
  for (int plane = 0; plane < num_planes; ++plane) {
    SkColorType plane_color_type = viz::ToClosestSkColorType(format, plane);
    gfx::Size plane_size = format.GetPlaneSize(plane, size);
    readback_bitmaps[plane].allocPixels(SkImageInfo::Make(
        plane_size.width(), plane_size.height(), plane_color_type, alpha_type));
  }
  std::vector<SkPixmap> readback_pixmaps = GetSkPixmaps(readback_bitmaps);

  // Readback.
  ASSERT_TRUE(backing->ReadbackToMemory(readback_pixmaps));

  // Verify.
  for (int plane = 0; plane < num_planes; ++plane) {
    EXPECT_TRUE(cc::MatchesBitmap(readback_bitmaps[plane],
                                  upload_bitmaps[plane],
                                  cc::ExactPixelComparator()))
        << "plane=" << plane;
  }
}

// Verify that creating and destroying a backing leaves whichever GL context and
// surface were current beforehand still current.
TEST_P(WrappedSkImageBackingFactoryTest, RestoresPreviouslyCurrentGLContext) {
  if (GetGrContextType() != GrContextType::kGL) {
    GTEST_SKIP() << "Only relevant when Skia uses GL";
  }

  auto format = GetFormat();
  gfx::Size size(100, 100);
  SharedImageInfo si_info(format, size, kColorSpace, kSurfaceOrigin,
                          GetAlphaType(format), kUsage, "TestLabel");

  // Use a different context and surface from the SharedContextState's.
  scoped_refptr<gl::GLSurface> other_surface =
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplayEGL(),
                                         gfx::Size(4, 4));
  ASSERT_TRUE(other_surface);
  scoped_refptr<gl::GLContext> other_context = gl::init::CreateGLContext(
      nullptr, other_surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(other_context);
  ASSERT_TRUE(other_context->MakeCurrent(other_surface.get()));

  auto backing = backing_factory_->CreateSharedImage(
      Mailbox::Generate(), si_info, gpu::kNullSurfaceHandle,
      /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);
  EXPECT_EQ(gl::GLContext::GetCurrent(), other_context.get());
  EXPECT_EQ(gl::GLSurface::GetCurrent(), other_surface.get());

  backing.reset();
  EXPECT_EQ(gl::GLContext::GetCurrent(), other_context.get());
  EXPECT_EQ(gl::GLSurface::GetCurrent(), other_surface.get());

  if (format.is_single_plane()) {
    std::vector<SkBitmap> bitmaps = AllocateRedBitmaps(format, size);
    auto pixels = gfx::SkPixmapToSpan(bitmaps[0].pixmap());
    backing = backing_factory_->CreateSharedImage(
        Mailbox::Generate(), si_info, /*is_thread_safe=*/false, pixels);
    ASSERT_TRUE(backing);
    EXPECT_EQ(gl::GLContext::GetCurrent(), other_context.get());
    EXPECT_EQ(gl::GLSurface::GetCurrent(), other_surface.get());

    backing.reset();
    EXPECT_EQ(gl::GLContext::GetCurrent(), other_context.get());
    EXPECT_EQ(gl::GLSurface::GetCurrent(), other_surface.get());
  }

  ASSERT_TRUE(gl_context_->MakeCurrent(gl_surface_.get()));
}

// Verify that WrappedSkImageBacking and WrappedGraphiteTextureBacking enforce
// exclusive write access and prevent concurrent read/write access across
// multiple representations, while allowing concurrent read access.
TEST_P(WrappedSkImageBackingFactoryTest, AccessExclusivity) {
  auto format = GetFormat();
  auto mailbox = Mailbox::Generate();
  gfx::Size size(100, 100);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox,
      {format, size, kColorSpace, kSurfaceOrigin, GetAlphaType(format), kUsage,
       "TestLabel"},
      gpu::kNullSurfaceHandle, /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);

  auto rep1 = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  auto rep2 = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  ASSERT_TRUE(rep1);
  ASSERT_TRUE(rep2);

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  // 1. Concurrent write access while write access is active must fail.
  auto write1 = rep1->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  ASSERT_TRUE(write1);
  EXPECT_FALSE(rep2->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes));
  EXPECT_FALSE(rep2->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes,
      /*use_sk_surface=*/false));

  // 2. Read access while write access is active must fail.
  rep1->SetCleared();
  EXPECT_FALSE(rep2->BeginScopedReadAccess(&begin_semaphores, &end_semaphores));

  write1.reset();

  // 3. Concurrent read access must succeed, and write access while read access
  // is active must fail.
  auto read1 = rep1->BeginScopedReadAccess(&begin_semaphores, &end_semaphores);
  ASSERT_TRUE(read1);
  auto read2 = rep2->BeginScopedReadAccess(&begin_semaphores, &end_semaphores);
  ASSERT_TRUE(read2);

  auto rep3 = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  ASSERT_TRUE(rep3);
  EXPECT_FALSE(rep3->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes));

  read1.reset();
  // Still one active reader (`read2`), so write access must still fail.
  EXPECT_FALSE(rep3->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes));

  read2.reset();
  // All readers finished; write access must now succeed.
  auto write3 = rep3->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(write3);
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
const auto kFormats = ::testing::Values(viz::SinglePlaneFormat::kR_8,
                                        viz::SinglePlaneFormat::kRG_88,
                                        viz::SinglePlaneFormat::kRGBA_4444,
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

#if BUILDFLAG(IS_WIN)
// ProduceVideo() with a D3D11 device is only supported on Windows. These tests
// verify the interaction between D3D video textures and
// WrappedGraphiteTextureBacking inside CompoundImageBacking.
class CompoundImageBackingWithWrappedSkImageTest
    : public WrappedSkImageBackingFactoryTest {
 public:
  void SetUp() override {
    WrappedSkImageBackingFactoryTest::SetUp();
    if (IsSkipped()) {
      return;
    }
    if (!context_state_->graphite_shared_context()) {
      // TODO(crbug.com/577947820): WrappedSkImageBacking does not implement
      // SupportsAccess() for kUseDynamicBackingAllocations.
      GTEST_SKIP()
          << "Requires WrappedSkImageBacking::SupportsAccess() on Ganesh";
    }
    scoped_feature_list_.InitAndEnableFeature(
        features::kUseDynamicBackingAllocations);
    factory_ = std::make_unique<SharedImageFactory>(
        gpu_preferences_, gpu_workarounds_, GpuFeatureInfo(),
        context_state_.get(), &shared_image_manager_, nullptr,
        /*is_for_display_compositor=*/false);
  }

  void TearDown() override {
    if (factory_) {
      factory_->DestroyAllSharedImages(true);
      factory_.reset();
    }
    WrappedSkImageBackingFactoryTest::TearDown();
  }

 protected:
  void VerifyD3D11VideoPixel(ID3D11Device* d3d11_device,
                             VideoImageRepresentation* representation,
                             base::span<const uint8_t, 4> expected_pixel) {
    auto read_access = representation->BeginScopedReadAccess();
    ASSERT_NE(read_access, nullptr);
    D3D11TextureAndArrayIndex video_texture = read_access->GetD3D11Texture();
    ASSERT_NE(video_texture.texture, nullptr);

    D3D11_TEXTURE2D_DESC desc = {};
    video_texture.texture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture;
    ASSERT_HRESULT_SUCCEEDED(
        d3d11_device->CreateTexture2D(&desc, nullptr, &staging_texture));

    Microsoft::WRL::ComPtr<ID3D11DeviceContext> device_context;
    d3d11_device->GetImmediateContext(&device_context);
    device_context->CopySubresourceRegion(staging_texture.Get(), 0, 0, 0, 0,
                                          video_texture.texture.Get(),
                                          video_texture.array_index, nullptr);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    ASSERT_HRESULT_SUCCEEDED(device_context->Map(staging_texture.Get(), 0,
                                                 D3D11_MAP_READ, 0, &mapped));
    // SAFETY: `staging_texture` is a 64x64 RGBA8 texture, so `mapped.pData` has
    // at least 4 bytes for the first pixel.
    auto first_pixel = UNSAFE_BUFFERS(
        base::span(static_cast<const uint8_t*>(mapped.pData), 4u));
    EXPECT_EQ(first_pixel, expected_pixel);
    device_context->Unmap(staging_texture.Get(), 0);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<SharedImageFactory> factory_;
};

// Verifies that calling ProduceVideo() on a SharedImage with dynamic backing
// allocation enabled reads the initialized pixels from the initial backing
// rather than uninitialized memory from the dynamically allocated backing.
TEST_P(CompoundImageBackingWithWrappedSkImageTest,
       ProduceVideoDynamicBackingAllocationReadsInitializedPixels) {
  constexpr gfx::Size size(64, 64);
  const auto format = GetFormat();
  constexpr gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  const gpu::Mailbox mailbox = gpu::Mailbox::Generate();

  constexpr std::array<uint8_t, 4> kGreen = {0, 255, 0, 255};
  std::vector<uint8_t> pixel_data(size.width() * size.height() * 4);
  for (size_t i = 0; i < pixel_data.size(); i += 4) {
    pixel_data[i + 0] = kGreen[0];
    pixel_data[i + 1] = kGreen[1];
    pixel_data[i + 2] = kGreen[2];
    pixel_data[i + 3] = kGreen[3];
  }

  SharedImageInfo si_info(format, size, kColorSpace, kSurfaceOrigin,
                          kPremul_SkAlphaType, usage,
                          "ProduceVideoDynamicTest");
  ASSERT_TRUE(factory_->CreateSharedImage(mailbox, si_info, pixel_data));

  auto d3d11_device = context_state_->GetD3D11Device();
  ASSERT_TRUE(d3d11_device);

  auto representation = shared_image_manager_.ProduceVideo(
      d3d11_device, mailbox, &memory_type_tracker_);
  ASSERT_NE(representation, nullptr);

  VerifyD3D11VideoPixel(d3d11_device.Get(), representation.get(), kGreen);

  representation.reset();
  EXPECT_TRUE(factory_->DestroySharedImage(mailbox));
}

// Verifies that calling ProduceVideo() on an initially uncleared SharedImage
// and then clearing it before BeginScopedReadAccess() reads the cleared pixels
// rather than uninitialized memory snapshotted during ProduceVideo().
TEST_P(CompoundImageBackingWithWrappedSkImageTest,
       ProduceVideoOnUnclearedSharedImageReadsLaterClearedPixels) {
  constexpr gfx::Size size(64, 64);
  const auto format = GetFormat();
  constexpr gpu::SharedImageUsageSet usage =
      gpu::SHARED_IMAGE_USAGE_RASTER_READ |
      gpu::SHARED_IMAGE_USAGE_RASTER_WRITE |
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ;
  const gpu::Mailbox mailbox = gpu::Mailbox::Generate();

  SharedImageInfo si_info(format, size, kColorSpace, kSurfaceOrigin,
                          kPremul_SkAlphaType, usage,
                          "ProduceVideoUnclearedDynamicTest");
  ASSERT_TRUE(
      factory_->CreateSharedImage(mailbox, si_info, kNullSurfaceHandle));

  auto d3d11_device = context_state_->GetD3D11Device();
  ASSERT_TRUE(d3d11_device);

  // Call ProduceVideo() while the SharedImage is still uncleared.
  auto representation = shared_image_manager_.ProduceVideo(
      d3d11_device, mailbox, &memory_type_tracker_);
  ASSERT_NE(representation, nullptr);
  EXPECT_FALSE(representation->IsCleared());

  // Clear the SharedImage to solid green via Skia before reading video.
  auto skia_rep = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  ASSERT_NE(skia_rep, nullptr);
  {
    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    auto skia_write = skia_rep->BeginScopedWriteAccess(
        &begin_semaphores, &end_semaphores,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_NE(skia_write, nullptr);
    skia_write->surface()->getCanvas()->clear(SkColors::kGreen);
    context_state_->FlushWriteAccess(skia_write.get());
    context_state_->SubmitIfNecessary(std::move(end_semaphores),
                                      skia_write->NeedGraphiteContextSubmit());
    skia_rep->SetCleared();
  }
  skia_rep.reset();
  EXPECT_TRUE(representation->IsCleared());

  constexpr std::array<uint8_t, 4> kGreen = {0, 255, 0, 255};
  VerifyD3D11VideoPixel(d3d11_device.Get(), representation.get(), kGreen);

  representation.reset();
  EXPECT_TRUE(factory_->DestroySharedImage(mailbox));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CompoundImageBackingWithWrappedSkImageTest,
    testing::Combine(testing::Values(viz::SinglePlaneFormat::kRGBA_8888),
                     testing::Values(GrContextType::kGL,
                                     GrContextType::kGraphiteDawn)),
    TestParamToString);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace
}  // namespace gpu
