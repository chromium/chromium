// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_factory_iosurface.h"

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

    GpuDriverBugWorkarounds workarounds;
    scoped_refptr<gl::GLShareGroup> share_group = new gl::GLShareGroup();
    context_state_ = base::MakeRefCounted<SharedContextState>(
        std::move(share_group), surface_, context_,
        false /* use_virtualized_gl_contexts */, base::DoNothing());
    context_state_->InitializeGrContext(workarounds, nullptr);
    auto feature_info =
        base::MakeRefCounted<gles2::FeatureInfo>(workarounds, GpuFeatureInfo());
    context_state_->InitializeGL(GpuPreferences(), std::move(feature_info));

    backing_factory_ = std::make_unique<SharedImageBackingFactoryIOSurface>(
        workarounds, GpuFeatureInfo(), /*use_gl*/ true);

    memory_type_tracker_ = std::make_unique<MemoryTypeTracker>(nullptr);
    shared_image_representation_factory_ =
        std::make_unique<SharedImageRepresentationFactory>(
            &shared_image_manager_, nullptr);
  }

  GrContext* gr_context() { return context_state_->gr_context(); }

 protected:
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<SharedImageBackingFactoryIOSurface> backing_factory_;
  gles2::MailboxManagerImpl mailbox_manager_;
  SharedImageManager shared_image_manager_;
  std::unique_ptr<MemoryTypeTracker> memory_type_tracker_;
  std::unique_ptr<SharedImageRepresentationFactory>
      shared_image_representation_factory_;
};

// Basic test to check creation and deletion of IOSurface backed shared image.
TEST_F(SharedImageBackingFactoryIOSurfaceTest, Basic) {
  Mailbox mailbox = Mailbox::GenerateForSharedImage();
  viz::ResourceFormat format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(256, 256);
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY;

  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, usage, false /* is_thread_safe */);
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
  base::Optional<SharedImageRepresentationSkia::ScopedWriteAccess>
      scoped_write_access;

  scoped_write_access.emplace(skia_representation.get(), &begin_semaphores,
                              &end_semaphores);
  auto* surface = scoped_write_access->surface();
  EXPECT_TRUE(surface);
  EXPECT_EQ(size.width(), surface->width());
  EXPECT_EQ(size.height(), surface->height());
  EXPECT_TRUE(begin_semaphores.empty());
  EXPECT_TRUE(end_semaphores.empty());
  scoped_write_access.reset();

  base::Optional<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access.emplace(skia_representation.get(), nullptr, nullptr);
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
  // Create a backing using mailbox.
  auto mailbox = Mailbox::GenerateForSharedImage();
  auto format = viz::ResourceFormat::RGBA_8888;
  gfx::Size size(1, 1);
  auto color_space = gfx::ColorSpace::CreateSRGB();
  uint32_t usage = SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_DISPLAY;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, usage, false /* is_thread_safe */);
  EXPECT_TRUE(backing);

  GLenum expected_target = GL_TEXTURE_RECTANGLE;
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
  gl_representation.reset();

  // Next create a SharedImageRepresentationSkia to read back the texture data.
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  EXPECT_TRUE(skia_representation);
  base::Optional<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access.emplace(skia_representation.get(), nullptr, nullptr);
  auto* promise_texture = scoped_read_access->promise_image_texture();
  EXPECT_TRUE(promise_texture);
    GrBackendTexture backend_texture = promise_texture->backendTexture();
    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());

  // Create an Sk Image from GrBackendTexture.
  auto sk_image = SkImage::MakeFromTexture(
      gr_context(), promise_texture->backendTexture(), kTopLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, kOpaque_SkAlphaType, nullptr);

  SkImageInfo dst_info =
      SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                        kOpaque_SkAlphaType, nullptr);

  const int num_pixels = size.width() * size.height();
  std::unique_ptr<uint8_t[]> dst_pixels(new uint8_t[num_pixels * 4]());

  // Read back pixels from Sk Image.
  EXPECT_TRUE(sk_image->readPixels(dst_info, dst_pixels.get(),
                                   dst_info.minRowBytes(), 0, 0));
  scoped_read_access.reset();

  // Compare the pixel values.
  EXPECT_EQ(dst_pixels[0], 0);
  EXPECT_EQ(dst_pixels[1], 255);
  EXPECT_EQ(dst_pixels[2], 0);
  EXPECT_EQ(dst_pixels[3], 255);

  skia_representation.reset();
  factory_ref.reset();
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
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
  uint32_t usage = SHARED_IMAGE_USAGE_WEBGPU | SHARED_IMAGE_USAGE_DISPLAY;
  auto backing = backing_factory_->CreateSharedImage(
      mailbox, format, size, color_space, usage, false /* is_thread_safe */);
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
    wgpu::Texture texture = wgpu::Texture::Acquire(
        dawn_representation->BeginAccess(WGPUTextureUsage_OutputAttachment));

    wgpu::RenderPassColorAttachmentDescriptor color_desc;
    color_desc.attachment = texture.CreateView();
    color_desc.resolveTarget = nullptr;
    color_desc.loadOp = wgpu::LoadOp::Clear;
    color_desc.storeOp = wgpu::StoreOp::Store;
    color_desc.clearColor = {0, 255, 0, 255};

    wgpu::RenderPassDescriptor renderPassDesc;
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &color_desc;
    renderPassDesc.depthStencilAttachment = nullptr;

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderPassDesc);
    pass.EndPass();
    wgpu::CommandBuffer commands = encoder.Finish();

    wgpu::Queue queue = device.CreateQueue();
    queue.Submit(1, &commands);
  }

  dawn_representation->EndAccess();

  // Next create a SharedImageRepresentationSkia to read back the texture data.
  auto skia_representation = shared_image_representation_factory_->ProduceSkia(
      mailbox, context_state_);
  EXPECT_TRUE(skia_representation);
  base::Optional<SharedImageRepresentationSkia::ScopedReadAccess>
      scoped_read_access;
  scoped_read_access.emplace(skia_representation.get(), nullptr, nullptr);
  auto* promise_texture = scoped_read_access->promise_image_texture();
  EXPECT_TRUE(promise_texture);
    GrBackendTexture backend_texture = promise_texture->backendTexture();
    EXPECT_TRUE(backend_texture.isValid());
    EXPECT_EQ(size.width(), backend_texture.width());
    EXPECT_EQ(size.height(), backend_texture.height());

  // Create an Sk Image from GrBackendTexture.
  auto sk_image = SkImage::MakeFromTexture(
      gr_context(), promise_texture->backendTexture(), kTopLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, kOpaque_SkAlphaType, nullptr);

  SkImageInfo dst_info =
      SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                        kOpaque_SkAlphaType, nullptr);

  const int num_pixels = size.width() * size.height();
  std::unique_ptr<uint8_t[]> dst_pixels(new uint8_t[num_pixels * 4]());

  // Read back pixels from Sk Image.
  EXPECT_TRUE(sk_image->readPixels(dst_info, dst_pixels.get(),
                                   dst_info.minRowBytes(), 0, 0));
  scoped_read_access.reset();

  // Compare the pixel values.
  EXPECT_EQ(dst_pixels[0], 0);
  EXPECT_EQ(dst_pixels[1], 255);
  EXPECT_EQ(dst_pixels[2], 0);
  EXPECT_EQ(dst_pixels[3], 255);

  // Shut down Dawn
  device = wgpu::Device();
  dawnProcSetProcs(nullptr);

  skia_representation.reset();
  factory_ref.reset();
  EXPECT_FALSE(mailbox_manager_.ConsumeTexture(mailbox));
}
#endif  // BUILDFLAG(USE_DAWN)

}  // anonymous namespace
}  // namespace gpu
