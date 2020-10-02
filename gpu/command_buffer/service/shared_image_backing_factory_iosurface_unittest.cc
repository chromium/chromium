// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"

#include <memory>
#include <utility>

#include "base/bind_helpers.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager_impl.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_factory.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/config/gpu_test_config.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory_io_surface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

#if BUILDFLAG(USE_DAWN)
#include <dawn/dawn_proc.h>
#include <dawn/webgpu_cpp.h>
#include <dawn_native/DawnNative.h>
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {
namespace {

class SharedImageBackingFactoryIOSurfaceTest : public testing::Test {
 public:
  void SetUp() override {
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
    ASSERT_TRUE(surface_);
    context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                         gl::GLContextAttribs());
    ASSERT_TRUE(context_);
    bool result = context_->MakeCurrent(surface_.get());
    ASSERT_TRUE(result);

    GpuPreferences preferences;
    preferences.texture_target_exception_list.push_back(
        gfx::BufferUsageAndFormat(gfx::BufferUsage::SCANOUT,
                                  gfx::BufferFormat::RGBA_8888));

    GpuDriverBugWorkarounds workarounds;
    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), surface_, context_,
        false /* use_virtualized_gl_contexts */, base::DoNothing());
    context_state_->InitializeGrContext(preferences, workarounds, nullptr);
    auto feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    context_state_->InitializeGL(preferences, std::move(feature_info));

    backing_factory_ = std::make_unique<SharedImageBackingFactoryGLTexture>(
        preferences, workarounds, GpuFeatureInfo(), &image_factory_,
        shared_image_manager_.batch_access_manager(),
        /*progress_reporter=*/nullptr);

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
  std::unique_ptr<SharedImageBackingFactoryGLTexture> backing_factory_;
  gles2::MailboxManagerImpl mailbox_manager_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  GpuMemoryBufferFactoryIOSurface image_factory_;

  void CheckSkiaPixels(const Mailbox& mailbox,
                       const gfx::Size& size,
                       const std::vector<uint8_t> expected_color) {
    auto skia_representation =
        shared_image_representation_factory_->ProduceSkia(mailbox,
                                                          context_state_);
    ASSERT_NE(skia_representation, nullptr);

    std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
        scoped_read_access =
            skia_representation->BeginScopedReadAccess(nullptr, nullptr);
    EXPECT_TRUE(scoped_read_access);

    auto* promise_texture = scoped_read_access->promise_image_texture();
    GrBackendTexture backend_texture = promise_texture->backendTexture();

    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());

    // Create an Sk Image from GrBackendTexture.
    auto sk_image = SkImage::MakeFromTexture(
        gr_context(), backend_texture, kTopLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, kOpaque_SkAlphaType, nullptr);

    const SkImageInfo dst_info =
        SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                          kOpaque_SkAlphaType, nullptr);

    const int num_pixels = size.width() * size.height();
    std::vector<uint8_t> dst_pixels(num_pixels * 4);

    // Read back pixels from Sk Image.
    EXPECT_TRUE(sk_image->readPixels(dst_info, dst_pixels.data(),
                                     dst_info.minRowBytes(), 0, 0));

    for (int i = 0; i < num_pixels; i++) {
      // Compare the pixel values.
      const uint8_t* pixel = dst_pixels.data() + (i * 4);
      EXPECT_EQ(pixel[0], expected_color[0]);
      EXPECT_EQ(pixel[1], expected_color[1]);
      EXPECT_EQ(pixel[2], expected_color[2]);
      EXPECT_EQ(pixel[3], expected_color[3]);
    }
  }
};

// Basic test to check creation and deletion of IOSurface backed shared image.
TEST_F(SharedImageBackingFactoryIOSurfaceTest, Basic) {
  // TODO(jonahr): Test crashes on Mac with ANGLE/passthrough
  // (crbug.com/1100980)
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("mac passthrough")) {
    return;
  }

  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  viz::ResourceFormat format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT;

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);

  // Check clearing.
  if (!backing->IsCleared()) {
    backing->SetCleared();
    EXPECT_TRUE(backing->IsCleared());
  }

  // First, validate via a legacy mailbox.
  GLenum expected_target = GL_TEXTURE_RECTANGLE;
  EXPECT_TRUE(backing->ProduceLegacyMailbox(&mailbox_manager_));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);

  // Currently there is no support for passthrough texture on Mac and hence
  // in IOSurface backing. So the TextureBase* should be pointing to a Texture
  // object.
  auto* texture = gles2::Texture::CheckedCast(texture_base);
  ASSERT_TRUE(texture);
  EXPECT_EQ(texture->target(), expected_target);
  EXPECT_TRUE(texture->IsImmutable());
  int width, height, depth;
  bool has_level =
      texture->GetLevelSize(GL_TEXTURE_2D, 0, &width, &height, &depth);
  EXPECT_TRUE(has_level);
  EXPECT_EQ(width, size.width());
  EXPECT_EQ(height, size.height());

  // Next validate via a SharedImageRepresentationGLTexture.
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());
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

  // Finally, validate a SharedImageRepresentationSkia.
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
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
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  scoped_write_access.reset();

  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access =
      skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  auto* promise_texture = scoped_read_access->promise_image_texture();
  EXPECT_TRUE(promise_texture);
    GrBackendTexture backend_texture = promise_texture->backendTexture();
    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());
    scoped_read_access.reset();
    skia_representation.reset();

    factory_ref.reset();
    EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

// Test to check interaction between Gl and skia GL representations.
// We write to a GL texture using gl representation and then read from skia
// representation.
TEST_F(SharedImageBackingFactoryIOSurfaceTest, GL_SkiaGL) {
  // TODO(jonahr): Test crashes on Mac with ANGLE/passthrough
  // (crbug.com/1100980)
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("mac passthrough")) {
    return;
  }

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);

  GLenum expected_target = GL_TEXTURE_RECTANGLE;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create a SharedImageRepresentationGLTexture.
  {
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexture(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());

    // Access the SharedImageRepresentationGLTexutre
    auto scoped_write_access = gl_representation->BeginScopedAccess(
        GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);

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
  }

  CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});
  factory_ref.reset();
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

// Test which ensures that legacy texture clear status is kept in sync with the
// SharedImageBacking.
TEST_F(SharedImageBackingFactoryIOSurfaceTest, LegacyClearing) {
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  viz::ResourceFormat format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;

  // Create a backing.
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);
  backing->SetCleared();
  EXPECT_TRUE(backing->IsCleared());

  // Also create a legacy mailbox.
  EXPECT_TRUE(backing->ProduceLegacyMailbox(&mailbox_manager_));
  TextureBase* texture_base = mailbox_manager_.ConsumeTexture(mailbox);
  auto* texture = gles2::Texture::CheckedCast(texture_base);
  EXPECT_TRUE(texture);
  GLenum target = texture->target();

  // Check initial state.
  EXPECT_TRUE(texture->IsLevelCleared(target, 0));
  EXPECT_TRUE(backing->IsCleared());

  // Un-clear the representation.
  backing->SetClearedRect(gfx::Rect());
  EXPECT_FALSE(texture->IsLevelCleared(target, 0));
  EXPECT_FALSE(backing->IsCleared());

  // Partially clear the representation.
  gfx::Rect partial_clear_rect(0, 0, 128, 128);
  backing->SetClearedRect(partial_clear_rect);
  EXPECT_EQ(partial_clear_rect, texture->GetLevelClearedRect(target, 0));
  EXPECT_EQ(partial_clear_rect, backing->ClearedRect());

  // Fully clear the representation.
  backing->SetCleared();
  EXPECT_TRUE(texture->IsLevelCleared(target, 0));
  EXPECT_TRUE(backing->IsCleared());

  // Un-clear the texture.
  texture->SetLevelClearedRect(target, 0, gfx::Rect());
  EXPECT_FALSE(texture->IsLevelCleared(target, 0));
  EXPECT_FALSE(backing->IsCleared());

  // Partially clear the texture.
  texture->SetLevelClearedRect(target, 0, partial_clear_rect);
  EXPECT_EQ(partial_clear_rect, texture->GetLevelClearedRect(target, 0));
  EXPECT_EQ(partial_clear_rect, backing->ClearedRect());

  // Fully clear the representation.
  texture->SetLevelCleared(target, 0, true);
  EXPECT_TRUE(texture->IsLevelCleared(target, 0));
  EXPECT_TRUE(backing->IsCleared());
}

#if BUILDFLAG(USE_DAWN)
// Test to check interaction between Dawn and skia GL representations.
TEST_F(SharedImageBackingFactoryIOSurfaceTest, Dawn_SkiaGL) {
  // Create a Dawn Metal device
  dawn_native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn_native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = std::find_if(
      adapters.begin(), adapters.end(), [](dawn_native::Adapter adapter) {
        return adapter.GetBackendType() == dawn_native::BackendType::Metal;
      });
  ASSERT_NE(adapter_it, adapters.end());

  wgpu::Device device = wgpu::Device::Acquire(adapter_it->CreateDevice());
  DawnProcTable procs = dawn_native::GetProcs();
  dawnProcSetProcs(&procs);

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  uint32_t usage = SHARED_IMAGE_USAGE_WEBGPU | SHARED_IMAGE_USAGE_SCANOUT;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create a SharedImageRepresentationDawn.
  auto dawn_representation =
      shared_image_representation_factory_->ProduceDawn(mailbox, device.Get());
  EXPECT_TRUE(dawn_representation);

  // Clear the shared image to green using Dawn.
  {
    auto scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_OutputAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(scoped_access);
    wgpu::Texture texture = wgpu::Texture::Acquire(scoped_access->texture());

    wgpu::RenderPassColorAttachmentDescriptor color_desc;
    color_desc.attachment = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearColor = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetDefaultQueue();
    queue.Submit(1, &commands);
  }

  CheckSkiaPixels(mailbox, size, {0, 255, 0, 255});

  // Shut down Dawn
  device = wgpu::Device();
  dawnProcSetProcs(nullptr);

  factory_ref.reset();
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}

// 1. Draw a color to texture through GL
// 2. Do not call SetCleared so we can test Dawn Lazy clear
// 3. Begin render pass in Dawn, but do not do anything
// 4. Verify through CheckSkiaPixel that GL drawn color not seen
TEST_F(SharedImageBackingFactoryIOSurfaceTest, GL_Dawn_Skia_UnclearTexture) {
  // TODO(jonahr): Test crashes on Mac with ANGLE/passthrough
  // (crbug.com/1100980)
  gpu::GPUTestBotConfig bot_config;
  if (bot_config.LoadCurrentConfig(nullptr) &&
      bot_config.Matches("mac passthrough")) {
    return;
  }

  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT |
                         SHARED_IMAGE_USAGE_WEBGPU;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);

  GLenum expected_target = GL_TEXTURE_RECTANGLE;
  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  {
    // Create a SharedImageRepresentationGLTexture.
    auto gl_representation =
        shared_image_representation_factory_->ProduceGLTexture(mailbox);
    EXPECT_TRUE(gl_representation);
    EXPECT_EQ(expected_target, gl_representation->GetTexture()->target());

    std::unique_ptr<SharedImageRepresentationGLTexturePassthrough::ScopedAccess>
        gl_scoped_access = gl_representation->BeginScopedAccess(
            GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM,
            SharedImageRepresentation::AllowUnclearedAccess::kYes);
    EXPECT_TRUE(gl_scoped_access);

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

    // Don't set cleared, we want to see if Dawn will lazy clear the texture
    EXPECT_FALSE(factory_ref->IsCleared());
  }

  // Create a Dawn Metal device
  dawn_native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn_native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = std::find_if(
      adapters.begin(), adapters.end(), [](dawn_native::Adapter adapter) {
        return adapter.GetBackendType() == dawn_native::BackendType::Metal;
      });
  ASSERT_NE(adapter_it, adapters.end());

  wgpu::Device device = wgpu::Device::Acquire(adapter_it->CreateDevice());
  DawnProcTable procs = dawn_native::GetProcs();
  dawnProcSetProcs(&procs);
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(mailbox,
                                                          device.Get());
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_OutputAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture =
        wgpu::Texture::Acquire(dawn_scoped_access->texture());
    wgpu::RenderPassColorAttachmentDescriptor color_desc;
    color_desc.attachment = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Load;
    color_desc.storeOp = wgpu::StoreOp::Store;

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetDefaultQueue();
    queue.Submit(1, &commands);
  }

  // Check skia pixels returns black since texture was lazy cleared in Dawn
  EXPECT_TRUE(factory_ref->IsCleared());
  CheckSkiaPixels(mailbox, size, {0, 0, 0, 0});

  // Shut down Dawn
  device = wgpu::Device();
  dawnProcSetProcs(nullptr);

  factory_ref.reset();
}

// 1. Draw  a color to texture through Dawn
// 2. Set the renderpass storeOp = Clear
// 3. Texture in Dawn will stay as uninitialized
// 3. Expect skia to fail to access the texture because texture is not
// initialized
TEST_F(SharedImageBackingFactoryIOSurfaceTest, UnclearDawn_SkiaFails) {
  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT |
                         SHARED_IMAGE_USAGE_WEBGPU;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  // Create dawn device
  dawn_native::Instance instance;
  instance.DiscoverDefaultAdapters();

  std::vector<dawn_native::Adapter> adapters = instance.GetAdapters();
  auto adapter_it = std::find_if(
      adapters.begin(), adapters.end(), [](dawn_native::Adapter adapter) {
        return adapter.GetBackendType() == dawn_native::BackendType::Metal;
      });
  ASSERT_NE(adapter_it, adapters.end());

  wgpu::Device device = wgpu::Device::Acquire(adapter_it->CreateDevice());
  DawnProcTable procs = dawn_native::GetProcs();
  dawnProcSetProcs(&procs);
  {
    auto dawn_representation =
        shared_image_representation_factory_->ProduceDawn(mailbox,
                                                          device.Get());
    ASSERT_TRUE(dawn_representation);

    auto dawn_scoped_access = dawn_representation->BeginScopedAccess(
        WGPUTextureUsage_OutputAttachment,
        SharedImageRepresentation::AllowUnclearedAccess::kYes);
    ASSERT_TRUE(dawn_scoped_access);

    wgpu::Texture texture =
        wgpu::Texture::Acquire(dawn_scoped_access->texture());
    wgpu::RenderPassColorAttachmentDescriptor color_desc;
    color_desc.attachment = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Clear;
    color_desc.clearColor = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc = {};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.GetDefaultQueue();
    queue.Submit(1, &commands);
  }

  // Shut down Dawn
  device = wgpu::Device();
  dawnProcSetProcs(nullptr);

  EXPECT_FALSE(factory_ref->IsCleared());

  // Produce skia representation
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);

  // Expect BeginScopedReadAccess to fail because sharedImage is uninitialized
  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  EXPECT_EQ(scoped_read_access, nullptr);
}
#endif  // BUILDFLAG(USE_DAWN)

// Test that Skia trying to access uninitialized SharedImage will fail
TEST_F(SharedImageBackingFactoryIOSurfaceTest, SkiaAccessFirstFails) {
  // Create a mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  const auto format = viz::ResourceFormat::RGBA_8888;
  const gfx::Size size(1, 1);
  const auto color_space = gfx::ColorSpace::CreateSRGB();
  GrSurfaceOrigin surface_origin = kTopLeft_GrSurfaceOrigin;
  SkAlphaType alpha_type = kPremul_SkAlphaType;
  const uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_SCANOUT;
  const gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, surface_handle, size, color_space, surface_origin,
      alpha_type, usage, false /* is_thread_safe */);
  ASSERT_NE(backing, nullptr);

  std::unique_ptr<SharedImageRepresentationFactoryRef> factory_ref =
      shared_image_manager_.Register(std::move(backing),
                                     memory_type_tracker_.get());

  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  ASSERT_NE(skia_representation, nullptr);
  EXPECT_FALSE(skia_representation->IsCleared());

  std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access =
          skia_representation->BeginScopedReadAccess(nullptr, nullptr);
  // Expect BeginScopedReadAccess to fail because sharedImage is uninitialized
  EXPECT_EQ(scoped_read_access, nullptr);
}
}  // anonymous namespace
}  // namespace gpu
