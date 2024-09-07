// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/angle_vulkan_image_backing_factory.h"

#include "base/no_destructor.h"
#include "base/test/scoped_feature_list.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/gpu/ganesh/GrBackendSemaphore.h"
#include "third_party/skia/include/private/chromium/GrPromiseImageTexture.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_switches.h"

namespace gpu {
namespace {

constexpr GrSurfaceOrigin kSurfaceOrigin = kTopLeft_GrSurfaceOrigin;
constexpr SkAlphaType kAlphaType = kPremul_SkAlphaType;
constexpr auto kColorSpace = gfx::ColorSpace::CreateSRGB();

// NOTE: The factory verifies that the usage for SIs created from empty GMBs
// includes GLES2 usage (either read or write) as the factory's entire purpose
// is GL-Vulkan interop, so it's necessary to specify *some* GLES2 usage here
// even though the tests don't actually use the GLES2 interface. The tests do
// exercise Skia read and write accesses, so include RASTER_{READ, WRITE} usage.
constexpr gpu::SharedImageUsageSet kUsage = SHARED_IMAGE_USAGE_RASTER_READ |
                                            SHARED_IMAGE_USAGE_RASTER_WRITE |
                                            SHARED_IMAGE_USAGE_GLES2_READ;

base::NoDestructor<base::test::ScopedFeatureList> g_scoped_feature_list;

// Must be run with --enable-features=DefaultANGLEVulkan so that ANGLE starts
// in right mode.
class AngleVulkanImageBackingFactoryTest
    : public SharedImageTestBase,
      public testing::WithParamInterface<viz::SharedImageFormat> {
 public:
  static void SetUpTestSuite() {
    // The DefaultANGLEVulkan feature needs to be enabled before the first test
    // runs to ensure ANGLE is started in the right mode. The static fixture
    // setup function accomplishes this.
    g_scoped_feature_list->InitWithFeatures(
        {features::kVulkan, features::kVulkanFromANGLE,
         features::kDefaultANGLEVulkan},
        {});

    // Reset vulkan function pointers so they can be reinitialized for ANGLE
    // Vulkan library.
    gpu::GetVulkanFunctionPointers()->ResetForTesting();
  }

  static void TearDownTestSuite() {
    g_scoped_feature_list->Reset();

    // Reset vulkan function pointers so they can be reinitialized for normal
    // Vulkan library.
    gpu::GetVulkanFunctionPointers()->ResetForTesting();
  }

  viz::SharedImageFormat GetFormat() { return GetParam(); }

  void SetUp() override {
    ASSERT_EQ(gl::GetGLImplementationParts(),
              gl::GLImplementationParts(gl::ANGLEImplementation::kVulkan));

    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kVulkan));
    backing_factory_ = std::make_unique<AngleVulkanImageBackingFactory>(
        gpu_preferences_, gpu_workarounds_, context_state_.get());
  }
};

// Verify creation and Skia access works as expected.
TEST_P(AngleVulkanImageBackingFactoryTest, Basic) {
  auto format = GetFormat();
  auto mailbox = Mailbox::Generate();
  gfx::Size size(100, 100);

  bool supported = backing_factory_->CanCreateSharedImage(
      kUsage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kVulkan, {});
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
  ASSERT_TRUE(skia_representation);

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
  auto scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  ASSERT_TRUE(scoped_read_access);
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());

  for (int plane = 0; plane < format.NumberOfPlanes(); ++plane) {
    auto* promise_texture = scoped_read_access->promise_image_texture(plane);
    ASSERT_TRUE(promise_texture);
    GrBackendTexture backend_texture = promise_texture->backendTexture();
    EXPECT_TRUE(backend_texture.isValid());

    auto plane_size = format.GetPlaneSize(plane, size);
    EXPECT_EQ(plane_size.width(), backend_texture.width());
    EXPECT_EQ(plane_size.height(), backend_texture.height());
  }

  scoped_read_access.reset();
  skia_representation.reset();
}

// Verify that pixel upload works as expected.
TEST_P(AngleVulkanImageBackingFactoryTest, Upload) {
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

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_ref =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  ASSERT_TRUE(shared_image_ref);

  VerifyPixelsWithReadbackGanesh(mailbox, bitmaps);
}

std::string TestParamToString(
    const testing::TestParamInfo<viz::SharedImageFormat>& param_info) {
  return param_info.param.ToTestParamString();
}

const auto kFormats = ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                                        viz::SinglePlaneFormat::kBGRA_8888,
                                        viz::SinglePlaneFormat::kR_8,
                                        viz::SinglePlaneFormat::kRG_88,
                                        viz::MultiPlaneFormat::kNV12,
                                        viz::MultiPlaneFormat::kYV12,
                                        viz::MultiPlaneFormat::kI420);

INSTANTIATE_TEST_SUITE_P(,
                         AngleVulkanImageBackingFactoryTest,
                         kFormats,
                         TestParamToString);

}  // namespace
}  // namespace gpu
