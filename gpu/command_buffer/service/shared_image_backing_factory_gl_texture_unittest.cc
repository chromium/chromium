// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"

#include <thread>

#include "base/callback_helpers.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
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
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_shared_memory.h"
#include "ui/gl/gl_image_stub.h"
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

bool IsAndroid() {
#if defined(OS_ANDROID)
  return true;
#else
  return false;
#endif
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
  SharedImageBackingFactoryGLTextureTestBase(bool is_thread_safe)
      : shared_image_manager_(
            std::make_unique<SharedImageManager>(is_thread_safe)) {}
  ~SharedImageBackingFactoryGLTextureTestBase() {
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
        preferences, workarounds, GpuFeatureInfo(), factory,
        shared_image_manager_->batch_access_manager(), &progress_reporter_);

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

  bool can_create_scanout_or_gmb_shared_image(
      viz::ResourceFormat format) const {
    if (format == viz::ResourceFormat::BGRA_1010102)
      return supports_ar30_;
    else if (format == viz::ResourceFormat::RGBA_1010102)
      return supports_ab30_;
    return true;
  }

  viz::ResourceFormat get_format() { return std::get<1>(GetParam()); }

  GrDirectContext* gr_context() { return context_state_->gr_context(); }

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

class SharedImageBackingFactoryGLTextureThreadSafeTest
    : public SharedImageBackingFactoryGLTextureTestBase {
 public:
  SharedImageBackingFactoryGLTextureThreadSafeTest()
      : SharedImageBackingFactoryGLTextureTestBase(true) {}
  ~SharedImageBackingFactoryGLTextureThreadSafeTest() {
    // |context_state2_| must be destroyed on its own context.
    context_state2_->MakeCurrent(surface2_.get(), true /* needs_gl */);
  }
  void SetUp() override {
    GpuDriverBugWorkarounds workarounds;
    workarounds.max_texture_size = INT_MAX - 1;
    SetUpBase(workarounds, &image_factory_);

    // Create 2nd context/context_state which are not part of same shared group.
    scoped_refptr<gles2::FeatureInfo> feature_info;
    CreateSharedContext(workarounds, surface2_, context2_, context_state2_,
                        feature_info);
    feature_info.reset();
  }

 protected:
  scoped_refptr<gl::GLSurface> surface2_;
  scoped_refptr<gl::GLContext> context2_;
  scoped_refptr<SharedContextState> context_state2_;
  TextureImageFactory image_factory_;
};

class CreateAndValidateSharedImageRepresentations {
 public:
  CreateAndValidateSharedImageRepresentations(
      SharedImageBackingFactoryGLTexture* backing_factory,
      viz::ResourceFormat format,
      bool is_thread_safe,
      gles2::MailboxManagerImpl* mailbox_manager,
      SharedImageManager* shared_image_manager,
      MemoryTypeTracker* memory_type_tracker,
      SharedImageRepresentationFactory* shared_image_representation_factory,
      SharedContextState* context_state);
  ~CreateAndValidateSharedImageRepresentations();

  gfx::Size size() { return size_; }
  Mailbox mailbox() { return mailbox_; }

 private:
  gles2::MailboxManagerImpl* mailbox_manager_;
  gfx::Size size_;
  Mailbox mailbox_;
  std::unique_ptr<SharedImageBacking> backing_;
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_;
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

TEST_P(SharedImageBackingFactoryGLTextureTest, Image) {
  // TODO(jonahr): Test crashes on Mac with ANGLE/passthrough
  // (crbug.com/1100975)
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("mac passthrough")) {
    return;
  }

  const bool should_succeed =
      can_create_scanout_or_gmb_shared_image(get_format());
  if (should_succeed)
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_SCANOUT;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);

  if (!should_succeed) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);
  ::testing::Mock::VerifyAndClearExpectations(&progress_reporter_);

  // Check clearing.
  if (!backing->IsCleared()) {
    backing->SetCleared();
    EXPECT_TRUE(backing->IsCleared());
  }

  // First, validate via a legacy mailbox.
  EXPECT_TRUE(backing->ProduceLegacyMailbox(&mailbox_manager_));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);
  ASSERT_TRUE(texture_base);
  GLenum target = texture_base->target();
  scoped_refptr<gl::GLImage> image;
  if (use_passthrough()) {
    auto* texture = static_cast<gles2::TexturePassthrough*>(texture_base);
    image = texture->GetLevelImage(target, 0);
  } else {
    auto* texture = static_cast<gles2::Texture*>(texture_base);
    image = texture->GetLevelImage(target, 0);
  }
  ASSERT_TRUE(image);
  EXPECT_EQ(size, image->GetSize());

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
    EXPECT_EQ(size, gl_representation->size());
    EXPECT_EQ(format, gl_representation->format());
    EXPECT_EQ(color_space, gl_representation->color_space());
    EXPECT_EQ(usage, gl_representation->usage());
    gl_representation.reset();

    auto gl_representation_rgb =
        shared_image_representation_factory_->ProduceRGBEmulationGLTexture(
            mailbox);
    EXPECT_TRUE(gl_representation_rgb);
    EXPECT_TRUE(gl_representation_rgb->GetTexture()->service_id());
    EXPECT_EQ(size, gl_representation_rgb->size());
    EXPECT_EQ(format, gl_representation_rgb->format());
    EXPECT_EQ(color_space, gl_representation_rgb->color_space());
    EXPECT_EQ(usage, gl_representation_rgb->usage());
    gl_representation_rgb.reset();
  }

  // Next, validate a SharedImageRepresentationGLTexturePassthrough.
  if (use_passthrough()) {
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
  auto* surface = scoped_write_access->surface();
  EXPECT_TRUE(surface);
  EXPECT_EQ(size.width(), surface->width());
  EXPECT_EQ(size.height(), surface->height());
  scoped_write_access.reset();

  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  auto* promise_texture = scoped_read_access->promise_image_texture();
  EXPECT_TRUE(promise_texture);
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  if (promise_texture) {
    GrBackendTexture backend_texture = promise_texture->backendTexture();
    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());
  }
  scoped_read_access.reset();
  skia_representation.reset();

  shared_image.reset();
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));

  if (!use_passthrough() &&
      context_state_->feature_info()->feature_flags().ext_texture_rg) {
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
    // Create a R-8 image texture, and check that the internal_format is that
    // of the image (GL_RGBA for TextureImageFactory). This only matters for
    // the validating decoder.
    auto format = viz::ResourceFormat::RED_8;
    gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
    backing = backing_factory_->CreateSharedImage(
        mailbox, format, surface_handle, size, color_space, surface_origin,
        alpha_type, usage, false /* is_thread_safe */);
    EXPECT_TRUE(backing);
    shared_image = shared_image_manager_->Register(std::move(backing),
                                                   memory_type_tracker_.get());
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexture(mailbox);
    ASSERT_TRUE(gl_representation);
    gles2::Texture* texture = gl_representation->GetTexture();
    ASSERT_TRUE(texture);
    GLenum type = 0;
    GLenum internal_format = 0;
    EXPECT_TRUE(texture->GetLevelType(target, 0, &type, &internal_format));
    EXPECT_EQ(internal_format, static_cast<GLenum>(GL_RGBA));
    gl_representation.reset();
    shared_image.reset();
  }
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
  const bool should_succeed =
      can_create_scanout_or_gmb_shared_image(get_format());
  if (should_succeed)
    EXPECT_CALL(progress_reporter_, ReportProgress).Times(AtLeast(1));
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = get_format();
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT;
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
    int storage_format = viz::TextureStorageFormat(format);

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

class StubImage : public gl::GLImageStub {
 public:
  StubImage(const gfx::Size& size, gfx::BufferFormat format)
      : size_(size), format_(format) {}

  gfx::Size GetSize() override { return size_; }
  unsigned GetInternalFormat() override {
    return gl::BufferFormatToGLInternalFormat(format_);
  }
  unsigned GetDataType() override {
    return gl::BufferFormatToGLDataType(format_);
  }

  BindOrCopy ShouldBindOrCopy() override { return BIND; }

  bool BindTexImage(unsigned target) override {
    if (!bound_) {
      bound_ = true;
      ++update_counter_;
    }
    return true;
  }

  bool BindTexImageWithInternalformat(unsigned target,
                                      unsigned internal_format) override {
    internal_format_ = internal_format;
    if (!bound_) {
      bound_ = true;
      ++update_counter_;
    }
    return true;
  }

  void ReleaseTexImage(unsigned target) override { bound_ = false; }

  bool bound() const { return bound_; }
  int update_counter() const { return update_counter_; }
  unsigned internal_format() const { return internal_format_; }

 private:
  ~StubImage() override = default;

  gfx::Size size_;
  gfx::BufferFormat format_;
  bool bound_ = false;
  int update_counter_ = 0;
  unsigned internal_format_ = GL_RGBA;
};

class SharedImageBackingFactoryGLTextureWithGMBTest
    : public SharedImageBackingFactoryGLTextureTestBase,
      public gpu::ImageFactory {
 public:
  SharedImageBackingFactoryGLTextureWithGMBTest()
      : SharedImageBackingFactoryGLTextureTestBase(false) {}
  void SetUp() override { SetUpBase(GpuDriverBugWorkarounds(), this); }

  scoped_refptr<gl::GLImage> GetImageFromMailbox(Mailbox mailbox) {
    if (!use_passthrough()) {
      auto representation =
          shared_image_representation_factory_->ProduceGLTexture(mailbox);
      DCHECK(representation);
      return representation->GetTexture()->GetLevelImage(GL_TEXTURE_2D, 0);
    } else {
      auto representation =
          shared_image_representation_factory_->ProduceGLTexturePassthrough(
              mailbox);
      DCHECK(representation);
      return representation->GetTexturePassthrough()->GetLevelImage(
          GL_TEXTURE_2D, 0);
    }
  }

 protected:
  // gpu::ImageFactory implementation.
  scoped_refptr<gl::GLImage> CreateImageForGpuMemoryBuffer(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      int client_id,
      gpu::SurfaceHandle surface_handle) override {
    // pretend to handle NATIVE_PIXMAP types.
    if (handle.type != gfx::NATIVE_PIXMAP)
      return nullptr;
    if (client_id != kClientId)
      return nullptr;
    return base::MakeRefCounted<StubImage>(size, format);
  }

  static constexpr int kClientId = 3;
};

TEST_P(SharedImageBackingFactoryGLTextureWithGMBTest,
       GpuMemoryBufferImportEmpty) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(256, 256);
  gfx::BufferFormat format = viz::BufferFormat(get_format());
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

  gfx::GpuMemoryBufferHandle handle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, kClientId, std::move(handle), format, kNullSurfaceHandle, size,
      color_space, surface_origin, alpha_type, usage);
  EXPECT_FALSE(backing);
}

TEST_P(SharedImageBackingFactoryGLTextureWithGMBTest,
       GpuMemoryBufferImportNative) {
  // TODO(jonahr): Test crashes on Mac with ANGLE/passthrough
  // (crbug.com/1100975)
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("mac passthrough")) {
    return;
  }
  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(256, 256);
  gfx::BufferFormat format = viz::BufferFormat(get_format());
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, kClientId, std::move(handle), format, kNullSurfaceHandle, size,
      color_space, surface_origin, alpha_type, usage);
  if (!can_create_scanout_or_gmb_shared_image(get_format())) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> ref =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  scoped_refptr<gl::GLImage> image = GetImageFromMailbox(mailbox);
  ASSERT_EQ(image->GetType(), gl::GLImage::Type::NONE);
  auto* stub_image = static_cast<StubImage*>(image.get());
  EXPECT_FALSE(stub_image->bound());
  int update_counter = stub_image->update_counter();
  ref->Update(nullptr);
  EXPECT_EQ(stub_image->update_counter(), update_counter);
  EXPECT_FALSE(stub_image->bound());

  {
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_);
    std::vector<GrBackendSemaphore> begin_semaphores;
    std::vector<GrBackendSemaphore> end_semaphores;
    std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
        scoped_read_access;
    skia_representation->BeginScopedReadAccess(&begin_semaphores,
                                               &end_semaphores);
  }
  EXPECT_TRUE(stub_image->bound());
  EXPECT_GT(stub_image->update_counter(), update_counter);
}

TEST_P(SharedImageBackingFactoryGLTextureWithGMBTest,
       GpuMemoryBufferImportSharedMemory) {
  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(256, 256);
  gfx::BufferFormat format = viz::BufferFormat(get_format());
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

  size_t shm_size = 0u;
  ASSERT_TRUE(gfx::BufferSizeForBufferFormatChecked(size, format, &shm_size));
  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.region = base::UnsafeSharedMemoryRegion::Create(shm_size);
  ASSERT_TRUE(handle.region.IsValid());
  handle.offset = 0;
  handle.stride = static_cast<int32_t>(
      gfx::RowSizeForBufferFormat(size.width(), format, 0));

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, kClientId, std::move(handle), format, kNullSurfaceHandle, size,
      color_space, surface_origin, alpha_type, usage);
  if (!can_create_scanout_or_gmb_shared_image(get_format())) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> ref =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());
  scoped_refptr<gl::GLImage> image = GetImageFromMailbox(mailbox);
  ASSERT_EQ(image->GetType(), gl::GLImage::Type::MEMORY);
  auto* shm_image = static_cast<gl::GLImageSharedMemory*>(image.get());
  EXPECT_EQ(size, shm_image->GetSize());
  EXPECT_EQ(format, shm_image->format());
}

TEST_P(SharedImageBackingFactoryGLTextureWithGMBTest,
       GpuMemoryBufferImportNative_WithRGBEmulation) {
  if (use_passthrough())
    return;
  auto mailbox = Mailbox::GenerateForSharedImage();
  gfx::Size size(256, 256);
  gfx::BufferFormat format = viz::BufferFormat(get_format());
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;

  gfx::GpuMemoryBufferHandle handle;
  handle.type = gfx::NATIVE_PIXMAP;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, kClientId, std::move(handle), format, kNullSurfaceHandle, size,
      color_space, surface_origin, alpha_type, usage);
  if (!can_create_scanout_or_gmb_shared_image(get_format())) {
    EXPECT_FALSE(backing);
    return;
  }
  ASSERT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> ref =
      shared_image_manager_->Register(std::move(backing),
                                      memory_type_tracker_.get());

  auto representation =
      shared_image_representation_factory_->ProduceRGBEmulationGLTexture(
          mailbox);
  EXPECT_TRUE(representation);
  EXPECT_TRUE(representation->GetTexture()->service_id());
  EXPECT_EQ(size, representation->size());
  EXPECT_EQ(get_format(), representation->format());
  EXPECT_EQ(color_space, representation->color_space());
  EXPECT_EQ(usage, representation->usage());

  scoped_refptr<gl::GLImage> image =
      representation->GetTexture()->GetLevelImage(GL_TEXTURE_2D, 0);
  ASSERT_EQ(image->GetType(), gl::GLImage::Type::NONE);
  auto* stub_image = static_cast<StubImage*>(image.get());
  EXPECT_EQ(stub_image->internal_format(), (unsigned)GL_RGB);
  EXPECT_TRUE(stub_image->bound());
  EXPECT_EQ(stub_image->update_counter(), 1);
}

// Intent of this test is to create at thread safe backing and test if all
// representations are working.
TEST_P(SharedImageBackingFactoryGLTextureThreadSafeTest, BasicThreadSafe) {
  // SharedImageBackingFactoryGLTextureThreadSafeTest tests are only meant for
  // android platform.
  if (!IsAndroid())
    return;

  CreateAndValidateSharedImageRepresentations shared_image(
      backing_factory_.get(), get_format(), true /* is_thread_safe */,
      &mailbox_manager_, shared_image_manager_.get(),
      memory_type_tracker_.get(), shared_image_representation_factory_.get(),
      context_state_.get());
}

// Intent of this test is to use the shared image mailbox system by 2 different
// threads each running their own GL context which are not part of same shared
// group. One thread will be writing to the backing and other thread will be
// reading from it.
TEST_P(SharedImageBackingFactoryGLTextureThreadSafeTest, OneWriterOneReader) {
  if (!IsAndroid())
    return;

  // Create it on 1st SharedContextState |context_state_|.
  CreateAndValidateSharedImageRepresentations shared_image(
      backing_factory_.get(), get_format(), true /* is_thread_safe */,
      &mailbox_manager_, shared_image_manager_.get(),
      memory_type_tracker_.get(), shared_image_representation_factory_.get(),
      context_state_.get());

  auto mailbox = shared_image.mailbox();
  auto size = shared_image.size();

  // Writer will write to the backing. We will create a GLTexture representation
  // and write green color to it.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexture(mailbox);
  EXPECT_TRUE(gl_representation);

  // Begin writing to the underlying texture of the backing via ScopedAccess.
  std::unique_ptr<SharedImageRepresentationGLTexture::ScopedAccess>
      writer_scoped_access = gl_representation->BeginScopedAccess(
          GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM,
          SharedImageRepresentation::AllowUnclearedAccess::kNo);

  DCHECK(writer_scoped_access);

  // Create an FBO.
  GLuint fbo = 0;
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGenFramebuffersEXTFn(1, &fbo);
  api->glBindFramebufferEXTFn(GL_FRAMEBUFFER, fbo);

  // Attach the texture to FBO.
  api->glFramebufferTexture2DEXTFn(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
      gl_representation->GetTexture()->target(),
      gl_representation->GetTexture()->service_id(), 0);

  // Set the clear color to green.
  api->glClearColorFn(0.0f, 1.0f, 0.0f, 1.0f);
  api->glClearFn(GL_COLOR_BUFFER_BIT);
  gl_representation->GetTexture()->SetLevelCleared(
      gl_representation->GetTexture()->target(), 0, true);

  // End writing.
  writer_scoped_access.reset();
  gl_representation.reset();

  // Read from the backing in a separate thread. Read is done via
  // SkiaGLRepresentation. ReadPixels() creates/produces a SkiaGLRepresentation
  // which in turn wraps a GLTextureRepresentation when for GL mode. Hence
  // testing reading via SkiaGLRepresentation is equivalent to testing via
  // GLTextureRepresentation.
  std::vector<uint8_t> dst_pixels;

  // Launch 2nd thread.
  std::thread second_thread([&]() {
    // Do ReadPixels() on 2nd SharedContextState |context_state2_|.
    dst_pixels = ReadPixels(mailbox, size, context_state2_.get(),
                            shared_image_representation_factory_.get());
  });

  // Wait for this thread to be done.
  second_thread.join();

  // Compare the pixel values.
  EXPECT_EQ(dst_pixels[0], 0);
  EXPECT_EQ(dst_pixels[1], 255);
  EXPECT_EQ(dst_pixels[2], 0);
  EXPECT_EQ(dst_pixels[3], 255);
}

CreateAndValidateSharedImageRepresentations::
    CreateAndValidateSharedImageRepresentations(
        SharedImageBackingFactoryGLTexture* backing_factory,
        viz::ResourceFormat format,
        bool is_thread_safe,
        gles2::MailboxManagerImpl* mailbox_manager,
        SharedImageManager* shared_image_manager,
        MemoryTypeTracker* memory_type_tracker,
        SharedImageRepresentationFactory* shared_image_representation_factory,
        SharedContextState* context_state)
    : mailbox_manager_(mailbox_manager), size_(256, 256) {
  // Make the context current.
  DCHECK(context_state);
  EXPECT_TRUE(
      context_state->MakeCurrent(context_state->surface(), true /* needs_gl*/));
  mailbox_ = Mailbox::GenerateForSharedImage();
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;

  // SHARED_IMAGE_USAGE_DISPLAY for skia read and SHARED_IMAGE_USAGE_RASTER for
  // skia write.
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_RASTER;
  if (!is_thread_safe)
    usage |= SHARED_IMAGE_USAGE_DISPLAY;
  backing_ = backing_factory->CreateSharedImage(
      mailbox_, format, surface_handle, size_, color_space, surface_origin,
      alpha_type, usage, is_thread_safe);

  // As long as either |chromium_image_ar30| or |chromium_image_ab30| is
  // enabled, we can create a non-scanout SharedImage with format
  // viz::ResourceFormat::{BGRA,RGBA}_1010102.
  const bool supports_ar30 =
      context_state->feature_info()->feature_flags().chromium_image_ar30;
  const bool supports_ab30 =
      context_state->feature_info()->feature_flags().chromium_image_ab30;
  if ((format == viz::ResourceFormat::BGRA_1010102 ||
       format == viz::ResourceFormat::RGBA_1010102) &&
      !supports_ar30 && !supports_ab30) {
    EXPECT_FALSE(backing_);
    return;
  }
  EXPECT_TRUE(backing_);
  if (!backing_)
    return;

  // Check clearing.
  if (!backing_->IsCleared()) {
    backing_->SetCleared();
    EXPECT_TRUE(backing_->IsCleared());
  }

  GLenum expected_target = GL_TEXTURE_2D;
  shared_image_ =
      shared_image_manager->Register(std::move(backing_), memory_type_tracker);

  // Create and validate GLTexture representation.
  auto gl_representation =
      shared_image_representation_factory->ProduceGLTexture(mailbox_);

  EXPECT_TRUE(gl_representation);
  EXPECT_TRUE(gl_representation->GetTexture()->service_id());
  EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());
  EXPECT_EQ(size_, gl_representation->size());
  EXPECT_EQ(format, gl_representation->format());
  EXPECT_EQ(color_space, gl_representation->color_space());
  EXPECT_EQ(usage, gl_representation->usage());
  gl_representation.reset();

  // Create and Validate Skia Representations.
  auto skia_representation =
      shared_image_representation_factory->ProduceSkia(mailbox_, context_state);
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kNo);
  // We use |supports_ar30| and |supports_ab30| to detect RGB10A2/BGR10A2
  // support. It's possible Skia might support these formats even if the Chrome
  // feature flags are false. We just check here that the feature flags don't
  // allow Chrome to do something that Skia doesn't support.
  if ((format != viz::ResourceFormat::BGRA_1010102 || supports_ar30) &&
      (format != viz::ResourceFormat::RGBA_1010102 || supports_ab30)) {
    EXPECT_TRUE(scoped_write_access);
    if (!scoped_write_access)
      return;
    auto* surface = scoped_write_access->surface();
    EXPECT_TRUE(surface);
    if (!surface)
      return;
    EXPECT_EQ(size_.width(), surface->width());
    EXPECT_EQ(size_.height(), surface->height());
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
  EXPECT_EQ(size_.width(), backend_texture.width());
  EXPECT_EQ(size_.height(), backend_texture.height());
  scoped_read_access.reset();
  skia_representation.reset();
}

CreateAndValidateSharedImageRepresentations::
    ~CreateAndValidateSharedImageRepresentations() {
  shared_image_.reset();
  EXPECT_FALSE(mailbox_manager_->ConsumeTexture(mailbox_));
}

#if !defined(OS_ANDROID)
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
INSTANTIATE_TEST_SUITE_P(Service,
                         SharedImageBackingFactoryGLTextureThreadSafeTest,
                         ::testing::Combine(::testing::Bool(),
                                            kResourceFormats),
                         TestParamToString);
INSTANTIATE_TEST_SUITE_P(Service,
                         SharedImageBackingFactoryGLTextureWithGMBTest,
                         ::testing::Combine(::testing::Bool(),
                                            kResourceFormats),
                         TestParamToString);

}  // anonymous namespace
}  // namespace gpu
