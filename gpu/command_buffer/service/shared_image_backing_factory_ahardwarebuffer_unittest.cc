// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_ahardwarebuffer.h"

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/callback_helpers.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_test_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace gpu {
namespace {

class SharedImageBackingFactoryAHBTest : public testing::Test {
 public:
  void SetUp() override {
    // AHardwareBuffer is only supported on ANDROID O+. Hence these tests
    // should not be run on android versions less that O.
    if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
      return;

    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    GpuDriverBugWorkarounds workarounds;
    workarounds.max_texture_size = INT_MAX - 1;

    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), surface_, context_,
        false /* use_virtualized_gl_contexts */, base::DoNothing());
    context_state_->InitializeGrContext(GpuPreferences(), workarounds, nullptr);
    auto feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    context_state_->InitializeGL(GpuPreferences(), std::move(feature_info));

    backing_factory_ = std::make_unique<SharedImageBackingFactoryAHB>(
        workarounds, GpuFeatureInfo());

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
  }

  GrDirectContext* gr_context() { return context_state_->gr_context(); }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageBackingFactoryAHB> backing_factory_;
  gles2::MailboxManagerImpl mailbox_manager_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
};

class GlLegacySharedImage {
 public:
  GlLegacySharedImage(
      SharedImageBackingFactoryAHB* backing_factory,
      bool is_thread_safe,
      gles2::MailboxManagerImpl* mailbox_manager,
      SharedImageManager* shared_image_manager,
      MemoryTypeTracker* memory_type_tracker,
      SharedImageRepresentationFactory* shared_image_representation_factory);
  ~GlLegacySharedImage();

  gfx::Size size() { return size_; }
  Mailbox mailbox() { return mailbox_; }

 private:
  gles2::MailboxManagerImpl* mailbox_manager_;
  gfx::Size size_;
  Mailbox mailbox_;
  std::unique_ptr<SharedImageBacking> backing_;
  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image_;
};

// Basic test to check creation and deletion of AHB backed shared image.
TEST_F(SharedImageBackingFactoryAHBTest, Basic) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),     false /* is_thread_safe */,
      &mailbox_manager_,          &shared_image_manager_,
      memory_type_tracker_.get(), shared_image_representation_factory_.get()};

  // Finally, validate a SharedImageRepresentationSkia.
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  EXPECT_TRUE(skia_representation);
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_write_access);
  auto* surface = scoped_write_access->surface();
  EXPECT_TRUE(surface);
  EXPECT_EQ(gl_legacy_shared_image.size().width(), surface->width());
  EXPECT_EQ(gl_legacy_shared_image.size().height(), surface->height());
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());
  scoped_write_access.reset();

  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(scoped_read_access);
  auto* promise_texture = scoped_read_access->promise_image_texture();
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());
  EXPECT_TRUE(promise_texture);
  GrBackendTexture backend_texture = promise_texture->backendTexture();
  EXPECT_TRUE(backend_texture.isValid());
  EXPECT_EQ(gl_legacy_shared_image.size().width(), backend_texture.width());
  EXPECT_EQ(gl_legacy_shared_image.size().height(), backend_texture.height());
  scoped_read_access.reset();
  skia_representation.reset();
}

// Test to check interaction between Gl and skia GL representations.
// We write to a GL texture using gl representation and then read from skia
// representation.
TEST_F(SharedImageBackingFactoryAHBTest, GLSkiaGL) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);

  GLenum expected_target = GL_TEXTURE_2D;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create a SharedImageRepresentationGLTexture.
  auto gl_representation =
      shared_image_representation_factory_->ProduceGLTexture(mailbox);
  EXPECT_TRUE(gl_representation);
  EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());

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

  // Mark the representation as cleared.
  gl_representation->SetCleared();
  gl_representation.reset();

  auto dst_pixels = ReadPixels(mailbox, size, context_state_.get(),
                               shared_image_representation_factory_.get());

  // Compare the pixel values.
  EXPECT_EQ(dst_pixels[0], 0);
  EXPECT_EQ(dst_pixels[1], 255);
  EXPECT_EQ(dst_pixels[2], 0);
  EXPECT_EQ(dst_pixels[3], 255);

  factory_ref.reset();
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

TEST_F(SharedImageBackingFactoryAHBTest, InitialData) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(4, 4);

  std::vector<uint8_t> initial_data(size.width() * size.height() * 4);

  for (size_t i = 0; i < initial_data.size(); i++) {
    initial_data[i] = static_cast<uint8_t>(i);
  }

  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      initial_data);
  EXPECT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  auto dst_pixels = ReadPixels(mailbox, size, context_state_.get(),
                               shared_image_representation_factory_.get());

  // Compare the pixel values.
  DCHECK(dst_pixels.size() == initial_data.size());

  EXPECT_EQ(dst_pixels, initial_data);
  factory_ref.reset();
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

// Test to check invalid format support.
TEST_F(SharedImageBackingFactoryAHBTest, InvalidFormat) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

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

// Test to check invalid size support.
TEST_F(SharedImageBackingFactoryAHBTest, InvalidSize) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
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

TEST_F(SharedImageBackingFactoryAHBTest, EstimatedSize) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);

  size_t backing_estimated_size = backing->estimated_size();
  EXPECT_GT(backing_estimated_size, 0u);

  std::unique_ptr<SharedImageRepresentationFactoryRef> shared_image =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());
  EXPECT_EQ(backing_estimated_size, memory_type_tracker_->GetMemRepresented());

  shared_image.reset();
}

// TODO(crbug/994720): Failing on Android builders.
// Test to check that only one context can write at a time
TEST_F(SharedImageBackingFactoryAHBTest, DISABLED_OnlyOneWriter) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),     true /* is_thread_safe */,
      &mailbox_manager_,          &shared_image_manager_,
      memory_type_tracker_.get(), shared_image_representation_factory_.get()};

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  auto skia_representation2 = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  std::vector<GrBackendSemaphore> begin_semaphores2;
  std::vector<GrBackendSemaphore> end_semaphores2;
  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_write_access2;
  scoped_write_access2 = skia_representation2->BeginScopedWriteAccess(
      &begin_semaphores2, &end_semaphores2,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_FALSE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores2.size());
  EXPECT_EQ(0u, end_semaphores2.size());
  skia_representation2.reset();

  scoped_write_access.reset();
  skia_representation.reset();
}

// Test to check that multiple readers are allowed
TEST_F(SharedImageBackingFactoryAHBTest, CanHaveMultipleReaders) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),     true /* is_thread_safe */,
      &mailbox_manager_,          &shared_image_manager_,
      memory_type_tracker_.get(), shared_image_representation_factory_.get()};

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  auto skia_representation2 = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(scoped_read_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access2;
  scoped_read_access2 = skia_representation2->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(scoped_read_access2);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  scoped_read_access2.reset();
  skia_representation2.reset();
  scoped_read_access.reset();
  skia_representation.reset();
}

// Test to check that a context cannot write while another context is reading
TEST_F(SharedImageBackingFactoryAHBTest, CannotWriteWhileReading) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),     true /* is_thread_safe */,
      &mailbox_manager_,          &shared_image_manager_,
      memory_type_tracker_.get(), shared_image_representation_factory_.get()};

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;

  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access = skia_representation->BeginScopedReadAccess(
      &begin_semaphores, &end_semaphores);
  EXPECT_TRUE(scoped_read_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  auto skia_representation2 = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores2;
  std::vector<GrBackendSemaphore> end_semaphores2;

  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation2->BeginScopedWriteAccess(
      &begin_semaphores2, &end_semaphores2,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_FALSE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores2.size());
  EXPECT_EQ(0u, end_semaphores2.size());
  skia_representation2.reset();

  scoped_read_access.reset();
  skia_representation.reset();
}

// Test to check that a context cannot read while another context is writing
TEST_F(SharedImageBackingFactoryAHBTest, CannotReadWhileWriting) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),     true /* is_thread_safe */,
      &mailbox_manager_,          &shared_image_manager_,
      memory_type_tracker_.get(), shared_image_representation_factory_.get()};

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_write_access;
  scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());

  auto skia_representation2 = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  std::vector<GrBackendSemaphore> begin_semaphores2;
  std::vector<GrBackendSemaphore> end_semaphores2;
  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access = skia_representation2->BeginScopedReadAccess(
      &begin_semaphores2, &end_semaphores2);
  EXPECT_FALSE(scoped_read_access);
  EXPECT_EQ(0u, begin_semaphores2.size());
  EXPECT_EQ(0u, end_semaphores2.size());
  skia_representation2.reset();

  scoped_write_access.reset();
  skia_representation.reset();
}

// Test to check that setting/unsetting legacy shared image mailboxes works as
// expected.
TEST_F(SharedImageBackingFactoryAHBTest, LegacyClearing) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),     false /* is_thread_safe */,
      &mailbox_manager_,          &shared_image_manager_,
      memory_type_tracker_.get(), shared_image_representation_factory_.get()};

  TextureBase* texture_base =
      mailbox_manager_.ConsumeTexture(gl_legacy_shared_image.mailbox());
  auto* texture = gles2::Texture::CheckedCast(texture_base);
  EXPECT_TRUE(texture);
  GLenum target = texture->target();

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());
  EXPECT_TRUE(skia_representation);

  // Check initial state.
  EXPECT_TRUE(texture->IsLevelCleared(target, 0));
  EXPECT_TRUE(skia_representation->IsCleared());

  // Un-clear the representation.
  skia_representation->SetClearedRect(gfx::Rect());
  EXPECT_FALSE(texture->IsLevelCleared(target, 0));
  EXPECT_FALSE(skia_representation->IsCleared());

  // Partially clear the representation.
  gfx::Rect partial_clear_rect(0, 0, 128, 128);
  skia_representation->SetClearedRect(partial_clear_rect);
  EXPECT_EQ(partial_clear_rect, texture->GetLevelClearedRect(target, 0));
  EXPECT_EQ(partial_clear_rect, skia_representation->ClearedRect());

  // Fully clear the representation.
  skia_representation->SetCleared();
  EXPECT_TRUE(texture->IsLevelCleared(target, 0));
  EXPECT_TRUE(skia_representation->IsCleared());

  // Un-clear the texture.
  texture->SetLevelClearedRect(target, 0, gfx::Rect());
  EXPECT_FALSE(texture->IsLevelCleared(target, 0));
  EXPECT_FALSE(skia_representation->IsCleared());

  // Partially clear the texture.
  texture->SetLevelClearedRect(target, 0, partial_clear_rect);
  EXPECT_EQ(partial_clear_rect, texture->GetLevelClearedRect(target, 0));
  EXPECT_EQ(partial_clear_rect, skia_representation->ClearedRect());

  // Fully clear the representation.
  texture->SetLevelCleared(target, 0, true);
  EXPECT_TRUE(texture->IsLevelCleared(target, 0));
  EXPECT_TRUE(skia_representation->IsCleared());
}

GlLegacySharedImage::GlLegacySharedImage(
    SharedImageBackingFactoryAHB* backing_factory,
    bool is_thread_safe,
    gles2::MailboxManagerImpl* mailbox_manager,
    SharedImageManager* shared_image_manager,
    MemoryTypeTracker* memory_type_tracker,
    SharedImageRepresentationFactory* shared_image_representation_factory)
    : mailbox_manager_(mailbox_manager), size_(256, 256) {
  mailbox_ = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
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
  EXPECT_TRUE(backing_);

  // Check clearing.
  if (!backing_->IsCleared()) {
    backing_->SetCleared();
    EXPECT_TRUE(backing_->IsCleared());
  }

  GLenum expected_target = GL_TEXTURE_2D;

  // First, validate via a legacy mailbox (only available when not
  // |is_thread_safe|).
  if (!is_thread_safe) {
    EXPECT_TRUE(backing_->ProduceLegacyMailbox(mailbox_manager_));

    TextureBase* texture_base = mailbox_manager_->ConsumeTexture(mailbox_);

    // Currently there is no support for passthrough texture on android and
    // hence in AHB backing. So the TextureBase* should be pointing to a Texture
    // object.
    auto* texture = gles2::Texture::CheckedCast(texture_base);
    EXPECT_TRUE(texture);
    EXPECT_EQ(texture->target(), expected_target);
    EXPECT_TRUE(texture->IsImmutable());
    int width, height, depth;
    bool has_level =
        texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, &depth);
    EXPECT_TRUE(has_level);
    EXPECT_EQ(width, size_.width());
    EXPECT_EQ(height, size_.height());
  } else {
    EXPECT_FALSE(backing_->ProduceLegacyMailbox(mailbox_manager_));
  }

  shared_image_ =
      shared_image_manager->Register(std::move(backing_), memory_type_tracker);

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
}

GlLegacySharedImage::~GlLegacySharedImage() {
  shared_image_.reset();
  EXPECT_FALSE(mailbox_manager_->ConsumeTexture(mailbox_));
}

TEST_F(SharedImageBackingFactoryAHBTest, Overlay) {
  if (!base::AndroidHardwareBufferCompat::IsSupportAvailable())
    return;

  GlLegacySharedImage gl_legacy_shared_image{
      backing_factory_.get(),     false /* is_thread_safe */,
      &mailbox_manager_,          &shared_image_manager_,
      memory_type_tracker_.get(), shared_image_representation_factory_.get()};

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      gl_legacy_shared_image.mailbox(), context_state_.get());

  std::vector<GrBackendSemaphore> begin_semaphores;
  std::vector<GrBackendSemaphore> end_semaphores;
  auto scoped_write_access = skia_representation->BeginScopedWriteAccess(
      &begin_semaphores, &end_semaphores,
      SharedImageRepresentation::AllowUnclearedAccess::kYes);
  EXPECT_TRUE(scoped_write_access);
  EXPECT_EQ(0u, begin_semaphores.size());
  EXPECT_EQ(0u, end_semaphores.size());
  scoped_write_access.reset();

  auto overlay_representation =
      shared_image_representation_factory_->ProduceOverlay(
          gl_legacy_shared_image.mailbox());
  EXPECT_TRUE(overlay_representation);

  auto scoped_read_access =
      overlay_representation->BeginScopedReadAccess(true /* needs_gl_image */);
  EXPECT_TRUE(scoped_read_access);
  EXPECT_TRUE(scoped_read_access->gl_image());
  auto buffer = scoped_read_access->gl_image()->GetAHardwareBuffer();
  DCHECK(buffer);
  scoped_read_access.reset();
  skia_representation.reset();
}

}  // anonymous namespace
}  // namespace gpu
