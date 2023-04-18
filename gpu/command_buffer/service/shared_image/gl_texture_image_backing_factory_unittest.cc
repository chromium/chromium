// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gl_texture_image_backing_factory.h"

#include <thread>

#include "base/command_line.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_test_base.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gl/progress_reporter.h"

using testing::AtLeast;

namespace gpu {
namespace {

bool IsGLSupported(viz::SharedImageFormat format) {
  return format.is_single_plane() && !format.IsLegacyMultiplanar() &&
         format != viz::SinglePlaneFormat::kBGR_565;
}

class MockProgressReporter : public gl::ProgressReporter {
 public:
  MockProgressReporter() = default;
  ~MockProgressReporter() override = default;

  // gl::ProgressReporter implementation.
  MOCK_METHOD0(ReportProgress, void());
};

class GLTextureImageBackingFactoryTestBase : public SharedImageTestBase {
 public:
  GLTextureImageBackingFactoryTestBase() = default;
  ~GLTextureImageBackingFactoryTestBase() override = default;

  void SetUpBase(bool for_cpu_upload_usage) {
    ASSERT_NO_FATAL_FAILURE(InitializeContext(GrContextType::kGL));
    auto* feature_info = context_state_->feature_info();

    // Check if platform should support various formats.
    supports_r_rg_ =
        feature_info->validators()->texture_format.IsValid(GL_RED_EXT) &&
        feature_info->validators()->texture_format.IsValid(GL_RG_EXT);
    supports_rg16_ =
        supports_r_rg_ &&
        feature_info->validators()->texture_internal_format.IsValid(
            GL_R16_EXT) &&
        feature_info->validators()->texture_internal_format.IsValid(
            GL_RG16_EXT);
    supports_rgba_f16_ =
        feature_info->validators()->pixel_type.IsValid(GL_HALF_FLOAT_OES);
    supports_etc1_ =
        feature_info->validators()->compressed_texture_format.IsValid(
            GL_ETC1_RGB8_OES);
    supports_ar30_ = feature_info->feature_flags().chromium_image_ar30;
    supports_ab30_ = feature_info->feature_flags().chromium_image_ab30;

    backing_factory_ = std::make_unique<GLTextureImageBackingFactory>(
        gpu_preferences_, gpu_workarounds_, context_state_->feature_info(),
        &progress_reporter_, for_cpu_upload_usage);
  }

  bool IsFormatSupport(viz::SharedImageFormat format) const {
    if (format.is_multi_plane()) {
      if (!use_passthrough()) {
        // Validating command decoder doesn't work with multi-planar textures.
        return false;
      }
      return supports_r_rg_;
    } else if (format == viz::SinglePlaneFormat::kR_8 ||
               format == viz::SinglePlaneFormat::kRG_88) {
      return supports_r_rg_;
    } else if (format == viz::SinglePlaneFormat::kR_16 ||
               format == viz::SinglePlaneFormat::kRG_1616) {
      return supports_rg16_;
    } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
      return supports_rgba_f16_;
    } else if (format == viz::SinglePlaneFormat::kBGRA_1010102 ||
               format == viz::SinglePlaneFormat::kRGBA_1010102) {
      return supports_ar30_ || supports_ab30_;
    } else if (format == viz::SinglePlaneFormat::kETC1) {
      return supports_etc1_;
    }
    return true;
  }

 protected:
  ::testing::NiceMock<MockProgressReporter> progress_reporter_;
  bool supports_r_rg_ = false;
  bool supports_rg16_ = false;
  bool supports_rgba_f16_ = false;
  bool supports_etc1_ = false;
  bool supports_ar30_ = false;
  bool supports_ab30_ = false;
};

// Non-parameterized tests.
class GLTextureImageBackingFactoryTest
    : public GLTextureImageBackingFactoryTestBase {
 public:
  void SetUp() override { SetUpBase(/*for_cpu_upload_usage=*/false); }
};

// SharedImageFormat parameterized tests.
class GLTextureImageBackingFactoryWithFormatTest
    : public GLTextureImageBackingFactoryTest,
      public testing::WithParamInterface<viz::SharedImageFormat> {
 public:
  viz::SharedImageFormat get_format() { return GetParam(); }
};

// SharedImageFormat parameterized tests for initial data upload. Only a subset
// of formats support upload.
using GLTextureImageBackingFactoryInitialDataTest =
    GLTextureImageBackingFactoryWithFormatTest;

// SharedImageFormat parameterized tests with a factory that supports pixel
// upload.
class GLTextureImageBackingFactoryWithUploadTest
    : public GLTextureImageBackingFactoryTestBase,
      public testing::WithParamInterface<viz::SharedImageFormat> {
 public:
  void SetUp() override { SetUpBase(/*for_cpu_upload_usage=*/true); }
  viz::SharedImageFormat get_format() { return GetParam(); }
};

using GLTextureImageBackingFactoryWithReadbackTest =
    GLTextureImageBackingFactoryWithUploadTest;

TEST_F(GLTextureImageBackingFactoryTest, InvalidFormat) {
  auto format = viz::LegacyMultiPlaneFormat::kNV12;
  gfx::Size size(256, 256);
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, {});
  EXPECT_FALSE(supported);
}

// Ensures that GLTextureImageBacking registers it's estimated size
// with memory tracker.
TEST_F(GLTextureImageBackingFactoryTest, EstimatedSize) {
  auto format = viz::SinglePlaneFormat::kRGBA_8888;
  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, {});
  ASSERT_TRUE(supported);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  size_t backing_estimated_size = backing->GetEstimatedSize();
  EXPECT_GT(backing_estimated_size, 0u);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_EQ(backing_estimated_size, memory_type_tracker_.GetMemRepresented());

  shared_image.reset();
}

// Ensures that the various conversion functions used w/ TexStorage2D match
// their TexImage2D equivalents, allowing us to minimize the amount of parallel
// data tracked in the GLTextureImageBackingFactory.
TEST_F(GLTextureImageBackingFactoryTest, TexImageTexStorageEquivalence) {
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(GpuDriverBugWorkarounds(), GpuFeatureInfo());
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                           use_passthrough(), gles2::DisallowedFeatures());
  const gles2::Validators* validators = feature_info->validators();

  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = viz::SharedImageFormat::SinglePlane(
        static_cast<viz::ResourceFormat>(i));
    if (!IsGLSupported(format) || format.IsCompressed())
      continue;
    int storage_format = TextureStorageFormat(
        format, feature_info->feature_flags().angle_rgbx_internal_format);

    int image_gl_format = GLDataFormat(format);
    int storage_gl_format =
        gles2::TextureManager::ExtractFormatFromStorageFormat(storage_format);
    EXPECT_EQ(image_gl_format, storage_gl_format);

    int image_gl_type = GLDataType(format);
    int storage_gl_type =
        gles2::TextureManager::ExtractTypeFromStorageFormat(storage_format);

    // Ignore the HALF_FLOAT / HALF_FLOAT_OES discrepancy for now.
    // TODO(ericrk): Figure out if we need additional action to support
    // HALF_FLOAT.
    if (!(image_gl_type == GL_HALF_FLOAT_OES &&
          storage_gl_type == GL_HALF_FLOAT)) {
      EXPECT_EQ(image_gl_type, storage_gl_type);
    }

    // confirm that we support TexStorage2D only if we support TexImage2D:
    int image_internal_format = GLInternalFormat(format);
    bool supports_tex_image =
        validators->texture_internal_format.IsValid(image_internal_format) &&
        validators->texture_format.IsValid(image_gl_format) &&
        validators->pixel_type.IsValid(image_gl_type);
    bool supports_tex_storage =
        validators->texture_internal_format_storage.IsValid(storage_format);
    if (supports_tex_storage)
      EXPECT_TRUE(supports_tex_image);
  }
}

TEST_P(GLTextureImageBackingFactoryWithFormatTest, IsSupported) {
  auto format = get_format();
  gfx::Size size(256, 256);
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, {});
  EXPECT_EQ(IsFormatSupport(format), supported);
}

TEST_P(GLTextureImageBackingFactoryWithFormatTest, Basic) {
  // TODO(jonahr): Test fails on Mac with ANGLE/passthrough
  // (crbug.com/1100975)
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("mac passthrough")) {
    GTEST_SKIP();
  }

  viz::SharedImageFormat format = get_format();
  if (!IsFormatSupport(format)) {
    GTEST_SKIP();
  }

#if BUILDFLAG(IS_IOS)
  if (format == viz::SinglePlaneFormat::kBGRA_1010102) {
    // Just like Mac, producing SkSurface for BGRA_1010102 fails on iOS
    // (crbug.com/1100975)
    GTEST_SKIP();
  }
#endif  // BUILDFLAG(IS_IOS)

  EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;

  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, {});
  ASSERT_TRUE(supported);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  // Check clearing.
  if (!backing->IsCleared()) {
    backing->SetCleared();
    EXPECT_TRUE(backing->IsCleared());
  }

  // First, validate via a GLTextureImageRepresentation.
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);
  GLenum expected_target = GL_TEXTURE_2D;
  if (!use_passthrough()) {
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexture(mailbox);
    EXPECT_TRUE(gl_representation);
    auto* texture = gl_representation->GetTexture(/*plane_index=*/0);
    EXPECT_TRUE(texture->service_id());
    EXPECT_EQ(expected_target, texture->target());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }

  // Next, validate a GLTexturePassthroughImageRepresentation.
  if (use_passthrough()) {
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    auto texture = gl_representation->GetTexturePassthrough(/*plane_index=*/0);
    EXPECT_TRUE(texture->service_id());
    EXPECT_EQ(expected_target, texture->target());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }

  // Finally, validate a SkiaImageRepresentation.
  auto skia_representation = shared_image_representation_factory_.ProduceSkia(
      mailbox, context_state_.get());
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SkiaImageRepresentation::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  // We use |supports_ar30_| and |supports_ab30_| to detect RGB10A2/BGR10A2
  // support. It's possible Skia might support these formats even if the Chrome
  // feature flags are false. We just check here that the feature flags don't
  // allow Chrome to do something that Skia doesn't support.
  if ((format != viz::SinglePlaneFormat::kBGRA_1010102 || supports_ar30_) &&
      (format != viz::SinglePlaneFormat::kRGBA_1010102 || supports_ab30_)) {
    ASSERT_TRUE(scoped_write_access);
    auto* surface = scoped_write_access->surface(/*plane_index=*/0);
    ASSERT_TRUE(surface);
    EXPECT_EQ(size.width(), surface->width());
    EXPECT_EQ(size.height(), surface->height());
  }
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  scoped_write_access.reset();

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

  shared_image.reset();
}

TEST_P(GLTextureImageBackingFactoryWithFormatTest, InvalidSize) {
  viz::SharedImageFormat format = get_format();
  if (!IsFormatSupport(format)) {
    GTEST_SKIP();
  }

  gfx::Size size(0, 0);
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, {});
  EXPECT_FALSE(supported);

  size = gfx::Size(INT_MAX, INT_MAX);
  supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, {});
  EXPECT_FALSE(supported);
}

TEST_P(GLTextureImageBackingFactoryInitialDataTest, InitialData) {
  viz::SharedImageFormat format = get_format();
  if (!IsFormatSupport(format)) {
    GTEST_SKIP();
  }

  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  std::vector<uint8_t> initial_data(
      viz::ResourceSizes::CheckedSizeInBytes<unsigned int>(size, format));

  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, initial_data);
  ASSERT_TRUE(supported);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      "TestLabel", initial_data);
  ASSERT_TRUE(backing);
  EXPECT_TRUE(backing->IsCleared());

  // Validate via a GLTextureImageRepresentation(Passthrough).
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing), &memory_type_tracker_);
  EXPECT_TRUE(shared_image);
  GLenum expected_target = GL_TEXTURE_2D;
  if (!use_passthrough()) {
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexture(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexture()->service_id());
    EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  } else {
    auto gl_representation =
        shared_image_representation_factory_.ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());
    EXPECT_EQ(expected_target,
              gl_representation->GetTexturePassthrough()->target());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }
}

TEST_P(GLTextureImageBackingFactoryInitialDataTest, InitialDataWrongSize) {
  viz::SharedImageFormat format = get_format();
  if (!IsFormatSupport(format)) {
    GTEST_SKIP();
  }

  gfx::Size size(256, 256);
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  size_t required_size =
      viz::ResourceSizes::CheckedSizeInBytes<size_t>(size, format);
  std::vector<uint8_t> initial_data_small(required_size / 2);
  std::vector<uint8_t> initial_data_large(required_size * 2);
  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size,
      /*thread_safe=*/false, gfx::EMPTY_BUFFER, GrContextType::kGL,
      initial_data_small);
  EXPECT_FALSE(supported);
  supported = backing_factory_->CanCreateSharedImage(
      usage, format, size,
      /*thread_safe=*/false, gfx::EMPTY_BUFFER, GrContextType::kGL,
      initial_data_large);
  EXPECT_FALSE(supported);
}

TEST_P(GLTextureImageBackingFactoryWithUploadTest, UploadFromMemory) {
  viz::SharedImageFormat format = get_format();
  if (!IsFormatSupport(format)) {
    GTEST_SKIP();
  }

  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(9, 9);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_CPU_UPLOAD;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;

  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, {});
  ASSERT_TRUE(supported);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  // Upload from bitmap with expected stride.
  {
    std::vector<SkBitmap> bitmaps =
        AllocateRedBitmaps(format, size, /*added_stride=*/0);
    EXPECT_TRUE(backing->UploadFromMemory(GetSkPixmaps(bitmaps)));
    EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }

  // Upload from bitmap with larger than expected stride.
  {
    std::vector<SkBitmap> bitmaps =
        AllocateRedBitmaps(format, size, /*added_stride=*/25);
    EXPECT_TRUE(backing->UploadFromMemory(GetSkPixmaps(bitmaps)));
    EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));
  }
}

TEST_P(GLTextureImageBackingFactoryWithReadbackTest, ReadbackToMemory) {
  viz::SharedImageFormat format = get_format();

  if (!IsFormatSupport(format)) {
    GTEST_SKIP();
  }

  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(9, 9);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_CPU_UPLOAD;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;

  bool supported = backing_factory_->CanCreateSharedImage(
      usage, format, size, /*thread_safe=*/false, gfx::EMPTY_BUFFER,
      GrContextType::kGL, {});
  ASSERT_TRUE(supported);

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, "TestLabel", /*is_thread_safe=*/false);
  ASSERT_TRUE(backing);

  std::vector<SkBitmap> src_bitmaps =
      AllocateRedBitmaps(format, size, /*added_stride=*/0);

  // Upload from bitmap with expected stride.
  ASSERT_TRUE(backing->UploadFromMemory(GetSkPixmaps(src_bitmaps)));
  EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));

  const int num_planes = format.NumberOfPlanes();

  {
    // Do readback into bitmap with same stride and validate pixels match what
    // was uploaded.
    std::vector<SkBitmap> readback_bitmaps(num_planes);
    for (int plane = 0; plane < num_planes; ++plane) {
      auto& info = src_bitmaps[plane].info();
      size_t stride = info.minRowBytes();
      readback_bitmaps[plane].allocPixels(info, stride);
    }

    std::vector<SkPixmap> pixmaps = GetSkPixmaps(readback_bitmaps);
    ASSERT_TRUE(backing->ReadbackToMemory(pixmaps));
    EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));

    for (int plane = 0; plane < num_planes; ++plane) {
      EXPECT_TRUE(cc::MatchesBitmap(readback_bitmaps[plane], src_bitmaps[plane],
                                    cc::ExactPixelComparator()))
          << "plane_index=" << plane;
    }
  }

  {
    // Do readback into a bitmap with larger than required stride and validate
    // pixels match what was uploaded.
    std::vector<SkBitmap> readback_bitmaps(num_planes);
    for (int plane = 0; plane < num_planes; ++plane) {
      auto& info = src_bitmaps[plane].info();
      size_t stride = info.minRowBytes() + 25 * info.bytesPerPixel();
      readback_bitmaps[plane].allocPixels(info, stride);
    }

    std::vector<SkPixmap> pixmaps = GetSkPixmaps(readback_bitmaps);
    ASSERT_TRUE(backing->ReadbackToMemory(pixmaps));
    EXPECT_EQ(glGetError(), static_cast<GLenum>(GL_NO_ERROR));

    for (int plane = 0; plane < num_planes; ++plane) {
      EXPECT_TRUE(cc::MatchesBitmap(readback_bitmaps[plane], src_bitmaps[plane],
                                    cc::ExactPixelComparator()))
          << "plane_index=" << plane;
    }
  }
}

std::string TestParamToString(
    const testing::TestParamInfo<viz::SharedImageFormat>& param_info) {
  return param_info.param.ToTestParamString();
}

const auto kInitialDataFormats =
    ::testing::Values(viz::SinglePlaneFormat::kETC1,
                      viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_8888,
                      viz::SinglePlaneFormat::kRGBA_4444,
                      viz::SinglePlaneFormat::kR_8,
                      viz::SinglePlaneFormat::kRG_88,
                      viz::SinglePlaneFormat::kBGRA_1010102,
                      viz::SinglePlaneFormat::kRGBA_1010102);

INSTANTIATE_TEST_SUITE_P(,
                         GLTextureImageBackingFactoryInitialDataTest,
                         kInitialDataFormats,
                         TestParamToString);

const auto kSharedImageFormats =
    ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_8888,
                      viz::SinglePlaneFormat::kRGBA_4444,
                      viz::SinglePlaneFormat::kR_8,
                      viz::SinglePlaneFormat::kRG_88,
                      viz::SinglePlaneFormat::kBGRA_1010102,
                      viz::SinglePlaneFormat::kRGBA_1010102,
                      viz::SinglePlaneFormat::kRGBX_8888,
                      viz::SinglePlaneFormat::kBGRX_8888,
                      viz::SinglePlaneFormat::kR_16,
                      viz::SinglePlaneFormat::kRG_1616,
                      viz::SinglePlaneFormat::kRGBA_F16,
                      viz::MultiPlaneFormat::kNV12,
                      viz::MultiPlaneFormat::kYV12);

INSTANTIATE_TEST_SUITE_P(,
                         GLTextureImageBackingFactoryWithFormatTest,
                         kSharedImageFormats,
                         TestParamToString);
INSTANTIATE_TEST_SUITE_P(,
                         GLTextureImageBackingFactoryWithUploadTest,
                         kSharedImageFormats,
                         TestParamToString);

const auto kReadbackFormats =
    ::testing::Values(viz::SinglePlaneFormat::kRGBA_8888,
                      viz::SinglePlaneFormat::kBGRA_8888,
                      viz::SinglePlaneFormat::kR_8,
                      viz::SinglePlaneFormat::kRG_88,
                      viz::SinglePlaneFormat::kRGBX_8888,
                      viz::SinglePlaneFormat::kBGRX_8888,
                      viz::MultiPlaneFormat::kNV12,
                      viz::MultiPlaneFormat::kYV12);

INSTANTIATE_TEST_SUITE_P(,
                         GLTextureImageBackingFactoryWithReadbackTest,
                         kReadbackFormats,
                         TestParamToString);

}  // anonymous namespace
}  // namespace gpu
