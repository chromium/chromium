// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"

#include <thread>

#include "base/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/service_utils.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_test_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/command_buffer/tests/texture_image_factory.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_test_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/progress_reporter.h"

using testing::AtLeast;

namespace gpu {
namespace {

void CreateSharedContext(const GpuDriverBugWorkarounds& workarounds,
                         scoped_refptr<gl::GLSurface>& surface,
                         scoped_refptr<gl::GLContext>& context,
                         scoped_refptr<SharedContextState>& context_state,
                         scoped_refptr<gles2::FeatureInfo>& feature_info) {
  surface = gl::init::CreateOffscreenGLSurface(gfx::Size());
  ASSERT_TRUE(surface);
  context =
      gl::init::CreateGLContext(nullptr, surface.get(), gl::GLContextAttribs());
  ASSERT_TRUE(context);
  bool result = context->MakeCurrent(surface.get());
  ASSERT_TRUE(result);

  scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
  feature_info =
      base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
  context_state = base::MakeRefCounted<SharedContextState>(
      std::move(share_group), surface, context,
      false /* use_virtualized_gl_contexts */, base::DoNothing());
  context_state->InitializeGrContext(GpuPreferences(), workarounds, nullptr);
  context_state->InitializeGL(GpuPreferences(), feature_info);
}

class MockProgressReporter : public gl::ProgressReporter {
 public:
  MockProgressReporter() = default;
  ~MockProgressReporter() override = default;

  // gl::ProgressReporter implementation.
  MOCK_METHOD0(ReportProgress, void());
};

class SharedImageBackingFactoryGLTextureTestBase
    : public testing::TestWithParam<std::tuple<bool, viz::ResourceFormat>> {
 public:
  explicit SharedImageBackingFactoryGLTextureTestBase(bool is_thread_safe)
      : shared_image_manager_(
            std::make_unique<SharedImageManager>(is_thread_safe)) {}
  ~SharedImageBackingFactoryGLTextureTestBase() override {
    // |context_state_| must be destroyed on its own context.
    context_state_->MakeCurrent(surface_.get(), true /* needs_gl */);
  }

  void SetUpBase(const GpuDriverBugWorkarounds& workarounds,
                 ImageFactory* factory) {
    scoped_refptr<gles2::FeatureInfo> feature_info;
    CreateSharedContext(workarounds, surface_, context_, context_state_,
                        feature_info);
    supports_etc1_ =
        feature_info->validators()->compressed_texture_format.IsValid(
            GL_ETC1_RGB8_OES);
    supports_ar30_ = feature_info->feature_flags().chromium_image_ar30;
    supports_ab30_ = feature_info->feature_flags().chromium_image_ab30;

    GpuPreferences preferences;
    preferences.use_passthrough_cmd_decoder = use_passthrough();
    backing_factory_ = std::make_unique<SharedImageBackingFactoryGLTexture>(
        preferences, workarounds, GpuFeatureInfo(), &progress_reporter_);

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            shared_image_manager_.get(), nullptr);
  }

  bool use_passthrough() {
    return std::get<0>(GetParam()) &&
           gles2::PassthroughCommandDecoderSupported();
  }

  bool can_create_non_scanout_shared_image(viz::ResourceFormat format) const {
    if (format == viz::ResourceFormat::BGRA_1010102 ||
        format == viz::ResourceFormat::RGBA_1010102) {
      return supports_ar30_ || supports_ab30_;
    } else if (format == viz::ResourceFormat::ETC1) {
      return supports_etc1_;
    }
    return true;
  }

  viz::ResourceFormat get_format() { return std::get<1>(GetParam()); }

 protected:
  ::testing::NiceMock<MockProgressReporter> progress_reporter_;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageBackingFactoryGLTexture> backing_factory_;
  gles2::MailboxManagerImpl mailbox_manager_;
  std::unique_ptr<SharedImageManager> shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  bool supports_etc1_ = false;
  bool supports_ar30_ = false;
  bool supports_ab30_ = false;
};

class SharedImageBackingFactoryGLTextureTest
    : public SharedImageBackingFactoryGLTextureTestBase {
 public:
  SharedImageBackingFactoryGLTextureTest()
      : SharedImageBackingFactoryGLTextureTestBase(false) {}
  void SetUp() override {
    GpuDriverBugWorkarounds workarounds;
    workarounds.max_texture_size = INT_MAX - 1;
    SetUpBase(workarounds, &image_factory_);
  }

 protected:
  TextureImageFactory image_factory_;
};

TEST_P(SharedImageBackingFactoryGLTextureTest, Basic) {
  // TODO(jonahr): Test fails on Mac with ANGLE/passthrough
  // (crbug.com/1100975)
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("mac passthrough")) {
    return;
  }

  const bool should_succeed = can_create_non_scanout_shared_image(get_format());
  if (should_succeed)
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);

  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  // Check clearing.
  if (!backing->IsCleared()) {
    backing->SetCleared();
    EXPECT_TRUE(backing->IsCleared());
  }

  // First, validate via a legacy mailbox.
  EXPECT_TRUE(backing->ProduceLegacyMailbox(&mailbox_manager_));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);
  ASSERT_TRUE(texture_base);
  GLenum expected_target = GL_TEXTURE_2D;
  EXPECT_EQ(texture_base->target(), expected_target);
  if (!use_passthrough()) {
    auto* texture = static_cast<gles2::Texture*>(texture_base);
    EXPECT_TRUE(texture->IsImmutable());
    int width, height, depth;
    bool has_level =
        texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, &depth);
    EXPECT_TRUE(has_level);
    EXPECT_EQ(width, size.width());
    EXPECT_EQ(height, size.height());
  }

  // Next, validate via a SharedImageRepresentationGLTexture.
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  EXPECT_TRUE(shared_image);
  if (!use_passthrough()) {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexture(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexture()->service_id());
    EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }

  // Next, validate a SharedImageRepresentationGLTexturePassthrough.
  if (use_passthrough()) {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
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

  // Finally, validate a SharedImageRepresentationSkia.
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_.get());
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  // We use |supports_ar30_| and |supports_ab30_| to detect RGB10A2/BGR10A2
  // support. It's possible Skia might support these formats even if the Chrome
  // feature flags are false. We just check here that the feature flags don't
  // allow Chrome to do something that Skia doesn't support.
  if ((format != viz::ResourceFormat::BGRA_1010102 || supports_ar30_) &&
      (format != viz::ResourceFormat::RGBA_1010102 || supports_ab30_)) {
    ASSERT_TRUE(scoped_write_access);
    auto* surface = scoped_write_access->surface();
    ASSERT_TRUE(surface);
    EXPECT_EQ(size.width(), surface->width());
    EXPECT_EQ(size.height(), surface->height());
  }
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  scoped_write_access.reset();

  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  auto* promise_texture = scoped_read_access->promise_image_texture();
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
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

TEST_P(SharedImageBackingFactoryGLTextureTest, InitialData) {
  // TODO(andrescj): these loop over the formats can be replaced by test
  // parameters.
  for (auto format :
       {viz::ResourceFormat::RGBA_8888, viz::ResourceFormat::ETC1,
        viz::ResourceFormat::BGRA_1010102, viz::ResourceFormat::RGBA_1010102}) {
    const bool should_succeed = can_create_non_scanout_shared_image(format);
    if (should_succeed)
      EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
    auto mailbox = Mailbox::GenerateForSharedImage();
    gfx::Size size(256, 256);
    auto color_space = gfx::ColorSpace::CreateSRGB();
    GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
    SkAlphaType alpha_type = kPremul_SkAlphaType;
    uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
    std::vector<uint8_t> initial_data(
        viz::ResourceSizes::CheckedSizeInBytes<unsigned int>(size, format));
    auto backing = backing_factory_->CreateSharedImage(
        mailbox, format, size, color_space, surface_origin, alpha_type, usage,
        initial_data);
    ::testing::Mock::VerifyAndClearExpectations(&progress_reporter_);
    if (!should_succeed) {
      EXPECT_FALSE(backing);
      continue;
    }
    ASSERT_TRUE(backing);
    EXPECT_TRUE(backing->IsCleared());

    // Validate via a SharedImageRepresentationGLTexture(Passthrough).
    std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
        shared_image_manager_->Register(std::move(backing),
                                        memory_type_tracker_.get());
    EXPECT_TRUE(shared_image);
    GLenum expected_target = GL_TEXTURE_2D;
    if (!use_passthrough()) {
      auto gl_representation =
          shared_image_representation_factory_->ProduceGLTexture(mailbox);
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
          shared_image_representation_factory_->ProduceGLTexturePassthrough(
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

    shared_image.reset();
    EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
  }
}

TEST_P(SharedImageBackingFactoryGLTextureTest, InitialDataImage) {
  const bool should_succeed = can_create_non_scanout_shared_image(get_format());
  if (should_succeed)
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  std::vector<uint8_t> initial_data(256 * 256 * 4);
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      initial_data);
  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  // Validate via a SharedImageRepresentationGLTexture(Passthrough).
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  EXPECT_TRUE(shared_image);
  if (!use_passthrough()) {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexture(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexture()->service_id());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  } else {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexturePassthrough(
            mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_TRUE(gl_representation->GetTexturePassthrough()->service_id());
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();
  }
}

TEST_P(SharedImageBackingFactoryGLTextureTest, InitialDataWrongSize) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  std::vector<uint8_t> initial_data_small(256 * 128 * 4);
  std::vector<uint8_t> initial_data_large(256 * 512 * 4);
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      initial_data_small);
  EXPECT_FALSE(backing);
  backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      initial_data_large);
  EXPECT_FALSE(backing);
}

TEST_P(SharedImageBackingFactoryGLTextureTest, InvalidFormat) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::YUV_420_BIPLANAR;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_FALSE(backing);
}

TEST_P(SharedImageBackingFactoryGLTextureTest, InvalidSize) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(0, 0);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_FALSE(backing);

  size = gfx::Size(INT_MAX, INT_MAX);
  backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_FALSE(backing);
}

TEST_P(SharedImageBackingFactoryGLTextureTest, EstimatedSize) {
  const bool should_succeed = can_create_non_scanout_shared_image(get_format());
  if (should_succeed)
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);

  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  size_t backing_estimated_size = backing->estimated_size();
  EXPECT_GT(backing_estimated_size, 0u);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  EXPECT_EQ(backing_estimated_size, memory_type_tracker_->GetMemRepresented());

  shared_image.reset();
}

// Ensures that the various conversion functions used w/ TexStorage2D match
// their TexImage2D equivalents, allowing us to minimize the amount of parallel
// data tracked in the SharedImageFactoryGLTexture.
TEST_P(SharedImageBackingFactoryGLTextureTest, TexImageTexStorageEquivalence) {
  scoped_refptr<gles2::FeatureInfo> feature_info =
      new gles2::FeatureInfo(GpuDriverBugWorkarounds(), GpuFeatureInfo());
  feature_info->Initialize(ContextType::CONTEXT_TYPE_OPENGLES2,
                           use_passthrough(), gles2::DisallowedFeatures());
  const gles2::Validators* validators = feature_info->validators();

  for (int i = 0; i <= viz::RESOURCE_FORMAT_MAX; ++i) {
    auto format = static_cast<viz::ResourceFormat>(i);
    if (!viz::GLSupportsFormat(format) ||
        viz::IsResourceFormatCompressed(format))
      continue;
    int storage_format = viz::TextureStorageFormat(
        format, feature_info->feature_flags().angle_rgbx_internal_format);

    int image_gl_format = viz::GLDataFormat(format);
    int storage_gl_format =
        gles2::TextureManager::ExtractFormatFromStorageFormat(storage_format);
    EXPECT_EQ(image_gl_format, storage_gl_format);

    int image_gl_type = viz::GLDataType(format);
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
    int image_internal_format = viz::GLInternalFormat(format);
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

#if !BUILDFLAG(IS_ANDROID)
const auto kResourceFormats =
    ::testing::Values(viz::ResourceFormat::RGBA_8888,
                      viz::ResourceFormat::BGRA_1010102,
                      viz::ResourceFormat::RGBA_1010102);
#else
// High bit depth rendering is not supported on Android.
const auto kResourceFormats = ::testing::Values(viz::ResourceFormat::RGBA_8888);
#endif

std::string TestParamToString(
    const testing::TestParamInfo<std::tuple<bool, viz::ResourceFormat>>&
        param_info) {
  const bool allow_passthrough = std::get<0>(param_info.param);
  const viz::ResourceFormat format = std::get<1>(param_info.param);
  return base::StringPrintf(
      "%s_%s", (allow_passthrough ? "AllowPassthrough" : "DisallowPassthrough"),
      gfx::BufferFormatToString(viz::BufferFormat(format)));
}

INSTANTIATE_TEST_SUITE_P(Service,
                         SharedImageBackingFactoryGLTextureTest,
                         ::testing::Combine(::testing::Bool(),
                                            kResourceFormats),
                         TestParamToString);

}  // anonymous namespace
}  // namespace gpu
